/* Copyright (C) 2011 Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "QtAudioDriver.h"

#include <mt32emu/mt32emu.h>

#include "../QSynth.h"

using namespace MT32Emu;

static const qint64 NANOS_PER_SECOND = 1000000000;

class WaveGenerator : public QIODevice {
private:
	QSynth *synth;
	QAudioOutput *audioOutput;
	double sampleRate;

public:
	WaveGenerator(QSynth *useSynth, QAudioOutput *useAudioOutput, double sampleRate) : synth(useSynth), audioOutput(useAudioOutput), sampleRate(sampleRate) {
		open(QIODevice::ReadOnly);
	}

	qint64 readData(char *data, qint64 len) {
		qint64 firstSampleNanos = audioOutput->processedUSecs() * 1000;
		return synth->render((Bit16s *)data, (unsigned int)(len >> 2), firstSampleNanos, sampleRate) << 2;
	}

	qint64 writeData(const char *data, qint64 len) {
		Q_UNUSED(data);
		Q_UNUSED(len);
		return 0;
	}
};

QtAudioDriver::QtAudioDriver(QSynth *useSynth, unsigned int useSampleRate) : sampleRate(useSampleRate) {
	QAudioFormat format;
	format.setFrequency(sampleRate);
	format.setChannels(2);
	format.setSampleSize(16);
	format.setCodec("audio/pcm");
	// libmt32emu produces samples in native byte order
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
	format.setByteOrder(QAudioFormat::LittleEndian);
#else
	format.setByteOrder(QAudioFormat::BigEndian);
#endif
	format.setSampleType(QAudioFormat::SignedInt);

	audioOutput = new QAudioOutput(format);
	waveGenerator = new WaveGenerator(useSynth, audioOutput, sampleRate);
}

QtAudioDriver::~QtAudioDriver() {
	delete audioOutput;
	delete waveGenerator;
}

/**
 * The granularity is terrible on Windows with the tested QAudioOutput version, but decent on ALSA.
 */
SynthTimestamp QtAudioDriver::getPlayedAudioNanosPlusLatency() {
	//return audioDriver->getPlayedAudioNanos();
	qint64 nsPlayed = audioOutput->processedUSecs() * 1000;
	qint64 bytesInBuffer = audioOutput->bufferSize() - audioOutput->bytesFree();
	qint64 nsInBuffer = NANOS_PER_SECOND * bytesInBuffer / (4 * sampleRate);
	nsPlayed -= nsInBuffer;
	return nsPlayed + latency;
}

bool QtAudioDriver::start(int deviceIndex) {
	Q_UNUSED(deviceIndex);
	audioOutput->start(waveGenerator);
	latency = 2 * NANOS_PER_SECOND * audioOutput->bufferSize() / (4 * sampleRate);
	return true;
}

void QtAudioDriver::close() {
	audioOutput->stop();
}
