
#include "vio.h"
#include <utilfuncs/utilfuncs.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <cmath>


//extern void telluser(const std::string s);



//===============================================================================voice


void Voice::MakeSineWave(double freq, double amplitude, double duration, double rate)
{
	clear();
	if ((freq<0.00125)||(amplitude<1.0)||(duration<0.00125)||(rate<10.0)) return; // 1/800 sec equivalent to u.p.s.=10 (rate==8000)
	double A=((amplitude>127.0)?127.0:amplitude), F, S=0.0, pi2=6.28318530718;
	size_t i, n=(size_t)(duration*rate);
	int k;
	F=(pi2*fmod(freq, (rate/2.0))/rate); //Nyquist - (fmod()): force frequency within possible limits for rate
	for (i=0; i<n; i++) { k=(A*sin(S)); (*this)+=(k<(-128))?0:(k>127)?127:(k+128); S+=F; }
}

void Voice::SetVolume(double dvol)
{
	int n;
	for (auto &u:(*this))
	{
		n=(int)(128.0+(double(u-128)*dvol));
		u=(n<0)?0:(n>255)?255:(uint8_t)n;
	}
}

void Voice::merge(const Voice &t) //, size_t pos=0)
{
	size_t i=0, n;
	int V,T,U;
	
	n=(size()<t.size())?size():t.size();
	for (i=0;i<n;i++)
	{
		V=(*this)[i];
		T=t[i];
		U=(V+T)/2;
		(*this)[i]=(VoiceTypeUnit)U;
	}
	while (i<t.size()) (*this)+=t[i++];
}

void Voice::unmerge(const Voice &t) //, size_t pos=0)
{
	size_t i=0, n;
	int V,T,U;
	
	n=(size()<t.size())?size():t.size();
	for (i=0;i<n;i++)
	{
		V=((*this)[i]*2);
		T=t[i];
		U=(V-T);
		(*this)[i]=(VoiceTypeUnit)U;
	}
}

void Voice::blendappend(const Voice &t)
{
	if (!size()) { (*this)=t; return; }
	if (!t.size()) return;

	size_t i=0, a, b;
	bool B=false, bvd, btd;

	a=(*this)[size()-2]; b=(*this)[size()-1]; bvd=((a-b)<0);
	while ((i<(t.size()-1))&&!B)
	{
		a=t[i]; b=t[i+1]; btd=((a-b)<0);
		if (!(B=(((bvd&&btd)&&(t[i]>(*this)[size()-1]))||((!bvd&&!btd)&&(t[i]<(*this)[size()-1]))))) i++;
	}
	for (;i<t.size();i++) (*this)+=t[i];

/*
	size_t i, At=0, Ab=0, Bt=0, Bb=0; //a, b;
	bool B=false, bvd, btd;
	int p;

	p=at(size()-1);
	for (i=(size()-1);((i>1)&&!B);i--)
	{
		if (!At&&((at(i)>p)&&(at(i)>at(i-1)))) { At=i; p=at[i]; }
		if (!Ab&&((at(i)<p)&&(at(i)<at(i-1)))) { Ab=i; p=at[i]; }
		B=(At&&Ab);
	}

	p=t.at(0);
	B=false;
	for (i=1;((i<(t.size()-1))&&!B);i++)
	{
		if (!Bt&&((t.at(i)>p)&&(t.at(i)>t.at(i+1)))) { Bt=i; p=t.at[i]; }
		if (!Bb&&((t.at(i)<p)&&(t.at(i)<t.at(i+1)))) { Bb=i; p=t.at[i]; }
		B=(Bt&&Bb);
	}

	if (At>Ab) //ends on At
	{
		if (Bt<Bb) //starts on Bt
		{
		}
		else //starts on Bb
		{
		}
	}
	else //ends on Ab
	{
		if (Bt<Bb) //starts on Bt
		{
		}
		else //starts on Bb
		{
		}
	}
*/

}


//===============================================================================threads

volatile bool bVPAUSE;
volatile bool bVSTOP;
volatile bool bVRECORDING;
volatile bool bVPLAYING;

std::mutex mtxVSTOP;

void getVStop(bool &b) { mtxVSTOP.lock(); b=bVSTOP; mtxVSTOP.unlock(); }
void setVStop(bool b) { mtxVSTOP.lock(); bVSTOP=b; mtxVSTOP.unlock(); }

void REC_Thread(VIO *pV)
{
	int i, rc;
	bool bSTOP=false;
	snd_pcm_uframes_t bs, period;
	snd_pcm_t *pPCM=nullptr;
	VoiceTypeUnit *pBuffer=nullptr;

	pV->sVIOERR.clear();
	
	if ((rc=snd_pcm_open(&pPCM, "default", SND_PCM_STREAM_CAPTURE, 0))>=0)
	{
		if ((rc=snd_pcm_set_params(pPCM, pV->Format, pV->Access, pV->Channels, pV->Rate, 1, 100000))>=0)
		{
			if ((rc=snd_pcm_get_params(pPCM, &bs, &period))>=0)
			{
				if ((pBuffer=(uint8_t*)malloc(period))!=nullptr)
				{
					bVRECORDING=true;
					pV->pVoice->clear();
					if (pV->cbVIO) pV->cbVIO(false);
					while (!bSTOP)
					{
						if ((rc=snd_pcm_readi(pPCM, pBuffer, period))<0) { pV->sVIOERR=spf("recording ERROR [", rc, "]: ", snd_strerror(rc)); bSTOP=true; }
						else for (i=0;i<rc;i++) (*(pV->pVoice))+=pBuffer[i];
						if (!bSTOP) getVStop(bSTOP);
					}
					bVRECORDING=false;
					free(pBuffer);
				} else { pV->sVIOERR="cannot create buffer"; }
			} else { pV->sVIOERR=spf("[", rc, "]: ", snd_strerror(rc)); }
		} else { pV->sVIOERR=spf("[", rc, "]: ", snd_strerror(rc)); }
		if (pPCM) snd_pcm_close(pPCM);
	} else { pV->sVIOERR=spf("[", rc, "]: ", snd_strerror(rc)); }
	if (pV->cbVIO) pV->cbVIO(true);
}


bool bVRESUME;
void PLAY_Thread(VIO *pV)
{
	static size_t idxplay=0;
	int rc, k;
	size_t m;
	bool bSTOP=false;
	snd_pcm_uframes_t bufsize, period;
	snd_pcm_t *pPCM=nullptr;
	char *pBuffer=nullptr; ///todo:...fix: use pre-allocated buffer
	
	pV->sVIOERR.clear();
	
	rc=snd_pcm_open(&pPCM, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc<0) { pV->sVIOERR=spf("[", rc, "]: ", snd_strerror(rc)); return; }
	
	rc=snd_pcm_set_params(pPCM, pV->Format, pV->Access, pV->Channels, pV->Rate, 1, 100000);
	if (rc<0) { if (pPCM) snd_pcm_close(pPCM); pV->sVIOERR=spf("[", rc, "]: ", snd_strerror(rc)); return; }

	rc=snd_pcm_get_params(pPCM, &bufsize, &period);
	if (rc<0) { if (pPCM) snd_pcm_close(pPCM); pV->sVIOERR=spf("[", rc, "]: ", snd_strerror(rc)); return; }
	
	bufsize*=2; //shitshow fails for orig bufsize or shorter sounds, so just double it
	if (!(pBuffer=(char*)malloc(bufsize))) { pV->sVIOERR="cannot create buffer"; return; }

	bVPLAYING=true;
	if (!bVRESUME) idxplay=0;
	bVRESUME=false;
	m=pV->pVoice->size();
	if (pV->cbVIO) pV->cbVIO(false);
	while (!bSTOP)
	{
		for (size_t i=0;i<bufsize;i++) { k=(idxplay+i); pBuffer[i]=(k<(int)m)?pV->pVoice->at(k):0; }
		if ((rc=snd_pcm_writei(pPCM, pBuffer, bufsize))<0)
		{
			pV->sVIOERR=spf("playing ERROR [", rc, "]: ", snd_strerror(rc)); bSTOP=true;
			//telluser(pV->sVIOERR.c_str());
		}
		idxplay+=bufsize; if (idxplay>=m) { bSTOP=true; idxplay=0; }
		if (!bSTOP) getVStop(bSTOP);
	}
	bVPLAYING=false;
	if (pBuffer) free(pBuffer);
	if (pPCM) snd_pcm_close(pPCM);
	if (pV->cbVIO) pV->cbVIO(false);
}


//===============================================================================VIO

VIO::VIO()
{
	sVIOERR.clear();
	pVoice=nullptr;
	cbVIO=nullptr;
	bVPAUSE=bVSTOP=bVRECORDING=bVPLAYING=false;
	bCanRec=bCanPlay=false;

	//this is good-enough for voice...
	Format=SND_PCM_FORMAT_U8;
	Access=SND_PCM_ACCESS_RW_INTERLEAVED;
	Channels=1;
	Rate=8000;

	check_setup();
}

VIO::~VIO() { Reset(); }

void VIO::check_setup()
{
	int rc;
	snd_pcm_t *pPCM;
	sVIOERR.clear();
	if ((bCanRec=((rc=snd_pcm_open(&pPCM, "default", SND_PCM_STREAM_CAPTURE, 0))>=0))) snd_pcm_close(pPCM); else sVIOERR=spf("[", rc, "]: ", snd_strerror(rc));
	if ((bCanPlay=((rc=snd_pcm_open(&pPCM, "default", SND_PCM_STREAM_PLAYBACK, 0))>=0))) snd_pcm_close(pPCM); else sVIOERR+=spf("[", rc, "]: ", snd_strerror(rc));
}

void VIO::Reset()
{
	cbVIO=nullptr;
	if (!IsReady()) { Stop(false); std::this_thread::sleep_for(std::chrono::milliseconds(50)); }
	sVIOERR.clear();
	pVoice=nullptr;
	bVPAUSE=bVSTOP=bVRECORDING=bVPLAYING=false;
	bCanRec=bCanPlay=false;
}

bool VIO::CanRecord()
{
	if (!bCanRec) check_setup();
	return (bCanRec&&!bVRECORDING&&!bVPLAYING&&!bVRESUME);
}
bool VIO::CanPlay()
{
	if (!bCanPlay) check_setup();
	return (bCanPlay&&(!bVRECORDING&&!bVPLAYING||bVRESUME));
}

bool VIO::Record(Voice *ps)
{
	if (CanRecord()&&ps)
	{
		bool bs;
		int l=0;
		pVoice=ps;
		bVSTOP=bVRECORDING=false;
		std::thread(REC_Thread, this).detach();
		getVStop(bs);
		while ((l<100)&&!bVRECORDING&&!bs) { l++; std::this_thread::sleep_for(std::chrono::milliseconds(5)); getVStop(bs); } //wait for thread to start
		//if (!sVIOERR.empty()) { telluser(sVIOERR); sVIOERR.clear(); } else return true;
		return (sVIOERR.empty());
	}
	return false;
}

bool VIO::Play(Voice *ps)
{
	if (CanPlay()&&ps)
	{
		bool bs;
		int l=0;
		sVIOERR.clear();
		pVoice=ps;
		bVPAUSE=bVSTOP=bVPLAYING=false;
		std::thread(PLAY_Thread, this).detach();
		do { l++; std::this_thread::sleep_for(std::chrono::milliseconds(5)); getVStop(bs); } while ((l<100)&&!bVPLAYING&&!bs);
		//if (!sVIOERR.empty()) { telluser(sVIOERR); sVIOERR.clear(); } else return true;
		return (sVIOERR.empty());
	}
	return false;
}

void VIO::Pause() { bVRESUME=bVPAUSE=true; Stop(); }

void VIO::Stop(bool bWait)
{
	if (!bVPAUSE) bVRESUME=false;
	bVPAUSE=false;
	setVStop(true);
	if (bWait) while (bVRECORDING||bVPLAYING) std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

bool VIO::IsRecording()	{ return bVRECORDING; }
bool VIO::IsPlaying()	{ return bVPLAYING; }
bool VIO::IsPaused()	{ return bVRESUME; }
bool VIO::IsReady()		{ return (CanRecord()&&CanPlay()); }



