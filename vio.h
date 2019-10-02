#ifndef _vio_h_
#define _vio_h_

//#include <utilfuncs/utilfuncs.h>

#include <string>
#include <alsa/asoundlib.h>
#include <vector>


template<typename T> struct DigitalSound : public std::vector<T>
{
	typedef std::vector<T> VT;

	void clear() { VT::clear(); }

	DigitalSound() { clear(); }
	DigitalSound(const DigitalSound &vt) { clear(); (*this)=vt; }
	~DigitalSound() { clear(); }

	DigitalSound& operator+=(const DigitalSound &vt) { (*this).insert((*this).end(), vt.begin(), vt.end()); return *this; }
	DigitalSound& operator+=(const T &t) { (*this).push_back(t); return *this; }


};


typedef uint8_t VoiceTypeUnit;


struct Voice : public DigitalSound<VoiceTypeUnit>
{
	//xxx
	
	Voice() { DigitalSound<VoiceTypeUnit>::clear(); }
	virtual ~Voice(){}
	bool empty() { return (DigitalSound<VoiceTypeUnit>::size()==0); }
	void MakeSineWave(double freq, double amplitude, double duration, double rate);
	void SetVolume(double dvol);
	void merge(const Voice &t);
	void unmerge(const Voice &t);
	void blendappend(const Voice &v);
	
};

//===============================================================================

typedef void (*VIOCallback)(bool /*true==rec/false==play*/);

struct VIO
{
	std::string sVIOERR;
	snd_pcm_format_t Format;
	snd_pcm_access_t Access;
	unsigned int Channels, Rate;
	Voice *pVoice; //set in calls to Record() & Play()
	bool bCanRec, bCanPlay;
	VIOCallback cbVIO; //called when rec/play has stopped
	
	void check_setup();

	VIO();
	~VIO();

	const std::string GetLastError() { return sVIOERR; }
	void Reset();
	void SetVIOCallback(VIOCallback cb=nullptr) { cbVIO=cb; }
	bool CanRecord();
	bool CanPlay();
	bool Record(Voice *pv);
	bool Play(Voice *pv);
	void Pause();
	void Stop(bool bWait=true);
	bool IsRecording();
	bool IsPlaying();
	bool IsPaused();
	bool IsReady();

	
};

void MakeSineWave(Voice &v, double freq, double amplitude, double duration, double unitspersecond);
void SetVolume(Voice &v, double dvol);



#endif
