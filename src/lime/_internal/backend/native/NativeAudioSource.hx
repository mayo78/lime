package lime._internal.backend.native;

import haxe.Int64;
import haxe.Timer;

import sys.thread.Thread;
import sys.thread.Mutex;

import lime.math.Vector2;
import lime.math.Vector4;
import lime.media.openal.AL;
import lime.media.openal.ALC;
import lime.media.openal.ALBuffer;
import lime.media.openal.ALSource;
import lime.media.AudioBuffer;
import lime.media.AudioDecoder;
import lime.media.AudioManager;
import lime.media.AudioSource;
import lime.utils.ArrayBuffer;
import lime.utils.ArrayBufferView;
import lime.utils.ArrayBufferView.ArrayBufferIO;
import lime.utils.UInt8Array;

@:access(haxe.Timer)
@:access(lime.media.AudioBuffer)
@:access(lime.media.AudioManager)
@:access(lime.media.AudioSource)
@:access(lime.utils.ArrayBufferView)
#if !lime_debug
@:fileXml('tags="haxe,release"')
@:noDebug
#end
class NativeAudioSource
{
	// how much buffers will be generating every frequency (doesnt have to be pow of 2?).
	public static var STREAM_BUFFER_SAMPLES:Int = 0x4000;

	// how much buffers can a stream hold on minimum or starting.
	public static var STREAM_MIN_BUFFERS:Int = 1;

	// how much limit of a buffers can be used for streamed audios, must be higher than minimum.
	public static var STREAM_MAX_BUFFERS:Int = 6;

	// how much buffers can it use to preserve previous datas.
	public static var STREAM_USABLE_BUFFERS:Int = 5;

	// how much buffers can it play.
	public static var STREAM_FLUSH_BUFFERS:Int = 4;

	// how much buffers can be processed in a frequency tick.
	public static var STREAM_PROCESS_BUFFERS:Int = 1;

	// how many seconds can it process buffers.
	public static var STREAM_PROCESS_DELAY:Float = 0.025;

	// how much buffers for the pool to hold.
	public static var POOL_MAX_BUFFERS:Int = 16;

	private static var bufferViewPool:Array<ArrayBufferView> = [];

	private static function resetTimer(timer:Timer, time:Float, callback:Void->Void):Timer
	@:privateAccess {
		if (timer == null) (timer = new Timer(time)).run = callback;
		else
		{
			timer.mTime = time;
			timer.mFireAt = Timer.getMS() + time;
			timer.mRunning = true;
			timer.run = callback;

			if (!Timer.sRunningTimers.contains(timer)) Timer.sRunningTimers.push(timer);
		}
		return timer;
	}

	private static var streamAudios:Array<NativeAudioSource> = [];
	private static var queuedStreamAudios:Array<NativeAudioSource> = [];
	private static var threadRunning:Bool = false;
	private static var streamThread:Thread;
	private static var streamMutex:Mutex = new Mutex();

	private var completed:Bool;
	private var format:Int;
	private var loops:Int;
	private var pauseSample:Int;
	private var peaks:Array<Float>;
	private var playing:Bool;
	private var position:Vector4;
	private var samples:Int;
	private var streamed:Bool;
	private var timer:Timer;

	public var parent:AudioSource;
	public var source:ALSource;
	public var loaded:Bool;

	private var standaloneBuffer:Bool;
	private var buffer:ALBuffer;
	private var standaloneDecoder:Bool;
	private var decoder:AudioDecoder;
	private var anglesArray:Array<Float>;
	private var loopPoints:Array<Int>;
	private var mins:Array<Int>;
	private var maxs:Array<Int>;

	public var bufferLen:Int;
	public var queuedBuffers:Int;
	public var filledBuffers:Int;
	public var internalQueuedBuffers:Int;
	public var streamLoops:Int;
	public var streamEnded:Bool;
	public var streaming:Bool;
	public var pending:Bool;

	// ORDERING IS CURRENT TO NEXT, STARTS FROM THE LENGTH OF THE ARRAYS
	public var bufferViews:Array<ArrayBufferView>;
	public var bufferCurs:Array<Int>;
	public var bufferLens:Array<Int>;

	public var mutex:Mutex;
	public var seekMutex:Mutex;
	private var buffers:Array<ALBuffer>;
	private var nextBuffer:Int = 0;

	public function new(parent:AudioSource)
	{
		this.parent = parent;
		init();
	}

	private function init():Void
	{
		if (source != null || (source = AL.createSource()) == null) return;
		AL.sourcef(source, AL.MAX_GAIN, 10);
		AL.sourcef(source, AL.MAX_DISTANCE, 1);

		loopPoints = [0, 0];
		anglesArray = [Math.PI / 6, -Math.PI / 6];

		if (AudioManager.__spatializeSupported) AL.sourcei(source, AL.SOURCE_SPATIALIZE_SOFT, AL.FALSE);
		if (AudioManager.__stereoAnglesSupported) AL.sourcefv(source, AL.STEREO_ANGLES, anglesArray);
	}

	public function dispose():Void
	{
		anglesArray = null;
		loopPoints = null;
		mins = null;
		maxs = null;

		if (source != null) AL.deleteSource(source);
		source = null;

		mutex = null;
		seekMutex = null;
	}

	public function load():Void
	{
		init();

		format = AudioBuffer.__getALFormat(parent.buffer.bitsPerSample, parent.buffer.channels);
		streamed = parent.buffer.data == null && parent.buffer.decoder != null;
		if (streamed)
		{
			samples = Int64.toInt(parent.buffer.decoder.total());

			if (mutex == null) mutex = new Mutex();
			if (seekMutex == null) seekMutex = new Mutex();
			mutex.acquire();
			seekMutex.acquire();

			decoder = parent.buffer.decoder.clone();
			standaloneDecoder = decoder != null;
			if (!standaloneDecoder) decoder = parent.buffer.decoder;

			buffers = AL.genBuffers(STREAM_FLUSH_BUFFERS);
			bufferLen = (STREAM_BUFFER_SAMPLES * parent.buffer.channels) * (parent.buffer.bitsPerSample >> 3);
			bufferCurs = [for (i in 0...STREAM_MAX_BUFFERS) 0];
			bufferLens = [for (i in 0...STREAM_MAX_BUFFERS) 0];
			bufferViews = [];

			bufferViews.resize(STREAM_MAX_BUFFERS);

			for (i in 0...STREAM_MAX_BUFFERS)
			{
				var data = bufferViewPool.pop();
				if (data == null) data = new UInt8Array(bufferLen);
				else
				{
					if (data.byteLength < bufferLen) data.buffer = new ArrayBuffer(bufferLen);
					data.byteLength = bufferLen;
					data.length = bufferLen;
				}
				bufferViews[i] = data;
			}

			loaded = true;
			mutex.release();
			seekMutex.release();
		}
		else if (parent.buffer.data != null)
		{
			samples = Std.int(parent.buffer.data.byteLength / (parent.buffer.bitsPerSample >> 3) / parent.buffer.channels);

			inline function createBuffer()
			{
				parent.buffer.__srcBuffer = AL.createBuffer();
				if (parent.buffer.__srcBuffer != null)
				{
					AL.bufferData(parent.buffer.__srcBuffer, format, parent.buffer.data, parent.buffer.data.byteLength, parent.buffer.sampleRate);
				}
			}

			if (parent.buffer.__srcBuffer == null)
			{
				createBuffer();
			}
			else if (AL.getBufferi(parent.buffer.__srcBuffer, AL.SIZE) != parent.buffer.data.byteLength)
			{
				// Corrupted ALBuffer, regenerate.
				AL.deleteBuffer(parent.buffer.__srcBuffer);
				createBuffer();
			}

			buffer = parent.buffer.__srcBuffer;
			loaded = buffer != null && source != null;
			if (loaded) AL.sourcei(source, AL.BUFFER, buffer);
		}
		else
		{
			loaded = false;
		}

		loopPoints[0] = 0;
		loopPoints[1] = samples;
		streamLoops = 0;
	}

	public function unload():Void
	{
		if (loaded)
		{
			if (streamed)
			{
				streamMutex.acquire();
				removeStream();
				queuedStreamAudios.remove(this);
				AL.sourceUnqueueBuffers(source, AL.getSourcei(source, AL.BUFFERS_QUEUED));
				internalQueuedBuffers = queuedBuffers = filledBuffers = 0;
				streamMutex.release();

				if (decoder != null && standaloneDecoder) decoder.dispose();
				decoder = null;
				standaloneDecoder = false;
			}
			else
			{
				AL.sourcei(source, AL.BUFFER, AL.NONE);
			}

			streamed = loaded = false;
		}

		if (standaloneBuffer && buffer != null) AL.deleteBuffer(buffer);
		standaloneBuffer = false;
		buffer = null;

		if (buffers != null) AL.deleteBuffers(buffers);
		buffers = null;

		if (bufferViews != null)
		{
			for (data in bufferViews) if (bufferViewPool.length < POOL_MAX_BUFFERS) bufferViewPool.push(data);
		}
		bufferCurs = null;
		bufferLens = null;
		bufferViews = null;

		loopPoints[0] = loopPoints[1] = 0;
		pauseSample = 0;
	}

	public function play():Void
	{
		if (!loaded || playing) return;

		playing = true;
		setCurrentTime(pauseSample * 1000.0 / parent.buffer.sampleRate);
	}

	public function pause():Void
	{
		if (!loaded || !playing) return;

		if (timer != null) timer.stop();
		playing = false;
		completed = false;
		pauseSample = getCurrentSampleOffset();

		AL.sourcePause(source);
		if (streamed) stopStream();
	}

	public function stop():Void
	{
		if (!loaded || !playing) return;

		if (timer != null) timer.stop();
		playing = false;
		completed = false;
		pauseSample = 0;

		AL.sourceStop(source);
		if (streamed) stopStream();
	}

	// Event Handlers
	private function complete():Void
	{
		if (!loaded) return;

		var sampleOffset = getCurrentSampleOffset();
		var remaining = (loopPoints[1] - sampleOffset) * 1000.0 / parent.buffer.sampleRate / getPitch();

		if (remaining > 30 && AL.getSourcei(source, AL.SOURCE_STATE) == AL.PLAYING && (!streaming || !streamEnded) && streamLoops == 0)
		{
			timer = resetTimer(timer, remaining, complete);
			return;
		}

		if (loops > 0)
		{
			loops--;
			remaining = (loopPoints[1] - loopPoints[0]) * 1000.0 / parent.buffer.sampleRate / getPitch();
			if (streamed) streamMutex.acquire();
			if (streamLoops > 0)
			{
				streamLoops--;
				timer = resetTimer(timer, remaining, complete);
				pauseSample = loopPoints[0];
			}
			else if (AudioManager.__loopPointsSupported && AL.getSourcei(source, AL.LOOPING) == AL.TRUE)
			{
				timer = resetTimer(timer, remaining, complete);
				pauseSample = loopPoints[0];
			}
			else
			{
				playing = true;
				setCurrentTime(loopPoints[0] * 1000.0 / parent.buffer.sampleRate - parent.offset);
			}

			if (streamed) streamMutex.release();
			else if (loops == 0) AL.sourcei(source, AL.LOOPING, AL.FALSE);
		}
		else
		{
			if (timer == null) timer.stop();
			completed = true;
			playing = false;
			pauseSample = 0;
		}

		parent.onComplete.dispatch();
	}

	// Get & Set Methods
	public function getCurrentTime():Float
	{
		return (getCurrentSampleOffset() * 1000.0 / parent.buffer.sampleRate) - parent.offset;
	}

	private function getCurrentSampleOffset():Int
	{
		if (!loaded) return 0;
		else if (completed) return loopPoints[1];
		else if (!playing) return pauseSample;

		var sampleOffset:Int;
		if (streamed)
		{
			seekMutex.acquire();
			if (queuedBuffers == 0) return pauseSample;

			sampleOffset = AL.getSourcei(source, AL.SAMPLE_OFFSET) + bufferCurs[STREAM_MAX_BUFFERS - queuedBuffers];
			if (AL.getSourcei(source, AL.SOURCE_STATE) == AL.STOPPED && internalQueuedBuffers == 0)
			{
				sampleOffset += STREAM_BUFFER_SAMPLES;
			}

			seekMutex.release();
		}
		else sampleOffset = AL.getSourcei(source, AL.SAMPLE_OFFSET);

		if (loops > streamLoops && sampleOffset >= loopPoints[1])
		{
			if (loopPoints[0] >= loopPoints[1]) return loopPoints[0];
			else return ((sampleOffset - loopPoints[0]) % (loopPoints[1] - loopPoints[0])) + loopPoints[0];
		}
		else
		{
			return sampleOffset;
		}
	}

	public function setCurrentTime(value:Float):Float
	{
		if (!loaded) return 0;

		var sampleOffset = Std.int((value + parent.offset) / 1000.0 * parent.buffer.sampleRate);
		if (sampleOffset < 0) sampleOffset = 0;
		else if (sampleOffset > loopPoints[1]) sampleOffset = loopPoints[1];

		pauseSample = sampleOffset;

		if (streamed)
		{
			mutex.acquire();
			AL.sourceStop(source);
		}
		else
		{
			AL.sourcei(source, AL.SAMPLE_OFFSET, sampleOffset);
		}

		if (playing)
		{
			completed = false;

			if (streamed)
			{
				snapBuffersToSample(sampleOffset, false, STREAM_MIN_BUFFERS);
				if (streamEnded) stopStream();
				else resetStream();
				AL.sourcePlay(source);
				mutex.release();
			}
			else if (AL.getSourcei(source, AL.SOURCE_STATE) != AL.PLAYING)
			{
				AL.sourcePlay(source);
			}
			timer = resetTimer(timer, (loopPoints[1] - sampleOffset) * 1000.0 / parent.buffer.sampleRate / getPitch(), complete);
		}
		else
		{
			if (timer != null) timer.stop();
			if (streamed)
			{
				AL.sourceUnqueueBuffers(source, AL.getSourcei(source, AL.BUFFERS_QUEUED));
				stopStream();
				mutex.release();
			}
			else
			{
				AL.sourcePause(source);
			}
		}

		return value;
	}

	public function getGain():Float
	{
		if (source != null) return AL.getSourcef(source, AL.GAIN);
		else return 1;
	}

	public function setGain(value:Float):Float
	{
		if (source != null) AL.sourcef(source, AL.GAIN, value);
		return value;
	}

	public function getLatency():Float
	{
		if (source != null && AudioManager.__latencyExtensionSupported)
		{
			var offsets = AL.getSourcedvSOFT(source, AL.SEC_OFFSET_LATENCY_SOFT, 2);
			if (offsets != null) return offsets[1] * 1000.0;
		}
		return 0;
	}

	public function getLength():Float
	{
		var length = loopPoints[1] * 1000.0 / parent.buffer.sampleRate;
		if (length <= parent.offset) return 0;
		return length - parent.offset;
	}

	public function setLength(value:Float):Float
	{
		if (loaded)
		{
			var lengthSample = Std.int((value + parent.offset) / 1000.0 * parent.buffer.sampleRate);
			if (lengthSample <= 0 || lengthSample >= samples) loopPoints[1] = samples;
			else loopPoints[1] = lengthSample;

			if (loops > streamLoops) updateLoopPoints();
			else AL.sourcei(source, AL.LOOPING, AL.FALSE);

			if (playing) timer = resetTimer(timer, (lengthSample - getCurrentSampleOffset()) * 1000.0 / parent.buffer.sampleRate / getPitch(), complete);
		}
		return value;
	}

	public function getLoopTime():Float
	{
		var loopTime = loopPoints[0] * 1000.0 / parent.buffer.sampleRate;
		if (loopTime <= parent.offset) return 0;
		return loopTime - parent.offset;
	}

	public function setLoopTime(value:Float):Float
	{
		if (loaded)
		{
			var loopSample = Std.int((value + parent.offset) / 1000.0 * parent.buffer.sampleRate);
			if (loopSample < 0) loopPoints[0] = 0;
			else if (loopSample > samples) loopPoints[0] = samples;
			else loopPoints[0] = loopSample;

			if (loops > streamLoops) updateLoopPoints();
			else AL.sourcei(source, AL.LOOPING, AL.FALSE);
		}
		return value;
	}

	public function getLoops():Int
	{
		return loops;
	}

	public function setLoops(value:Int):Int
	{
		loops = value;
		if (loaded)
		{
			if (value > streamLoops) updateLoopPoints();
			else AL.sourcei(source, AL.LOOPING, AL.FALSE);
		}
		return value;
	}

	private function updateLoopPoints():Void
	{
		var sampleOffset = getCurrentSampleOffset();
		var canLoop = loops > streamLoops;
		var fixed = sampleOffset >= loopPoints[1];
		var shouldStop = playing && fixed;

		if (fixed) sampleOffset = loopPoints[0];
		var time = sampleOffset * 1000.0 / parent.buffer.sampleRate;

		if (streamed)
		{
			mutex.acquire();

			AL.sourcei(source, AL.LOOPING, AL.FALSE);
			if (shouldStop) stop();
			else if (playing && canLoop && (fixed || streamLoops > 0))
			{
				AL.sourceStop(source);
				snapBuffersToSample(sampleOffset, streamLoops > 0, STREAM_MIN_BUFFERS);
				AL.sourcePlay(source);
			}

			mutex.release();
		}
		else
		{
			if (loopPoints[0] > 0 || loopPoints[1] < samples)
			{
				if (!AudioManager.__loopPointsSupported) canLoop = false;
				else
				{
					AL.sourceStop(source);
					AL.sourcei(source, AL.BUFFER, AL.NONE);
					if (!standaloneBuffer)
					{
						if (standaloneBuffer = (buffer = AL.createBuffer()) != null)
						{
							AL.bufferData(buffer, format, parent.buffer.data, parent.buffer.data.byteLength, parent.buffer.sampleRate);
						}
						else
						{
							buffer = parent.buffer.__srcBuffer;
							canLoop = false;
						}
					}
					if (canLoop) AL.bufferiv(buffer, AL.LOOP_POINTS_SOFT, loopPoints);
					AL.sourcei(source, AL.BUFFER, buffer);
				}
			}

			AL.sourcei(source, AL.LOOPING, canLoop ? AL.TRUE : AL.FALSE);
			if (shouldStop) stop();
			else setCurrentTime(time);
		}
	}

	public function getPan():Float
	{
		if (position == null) position = new Vector4();
		return position.x;
	}

	public function setPan(value:Float):Float
	{
		if (position == null) position = new Vector4();
		position.setTo(value, 0, -Math.sqrt(1 - value * value));

		if (source != null)
		{
			if (AudioManager.__stereoAnglesSupported)
			{
				anglesArray[0] = Math.PI * Math.min(-value * 2 + 1, 1) / 6;
				anglesArray[1] = -Math.PI * Math.min(value * 2 + 1, 1) / 6;
				if (AudioManager.__spatializeSupported)
				{
					AL.sourcei(source, AL.SOURCE_SPATIALIZE_SOFT, AL.FALSE);
				}
				AL.source3f(source, AL.POSITION, 0, 0, 0);
			}
			else
			{
				anglesArray[0] = Math.PI / 6;
				anglesArray[1] = -Math.PI / 6;
				if (AudioManager.__spatializeSupported)
				{
					AL.sourcei(source, AL.SOURCE_SPATIALIZE_SOFT, Math.abs(value) > 1e-04 ? AL.TRUE : AL.FALSE);
				}
				AL.source3f(source, AL.POSITION, position.x, position.y, position.z);
			}
			AL.sourcefv(source, AL.STEREO_ANGLES, anglesArray);
		}
		return value;
	}

	public function getPitch():Float
	{
		if (source != null) return AL.getSourcef(source, AL.PITCH);
		else return 1;
	}

	public function setPitch(value:Float):Float
	{
		value = Math.max(value, 0);
		if (source == null || value == AL.getSourcef(source, AL.PITCH)) return value;
		AL.sourcef(source, AL.PITCH, value);
		if (playing) timer = resetTimer(timer, (loopPoints[1] - getCurrentSampleOffset()) * 1000.0 / parent.buffer.sampleRate / value, complete);
		return value;
	}

	public function getPlaying():Bool
	{
		return playing;
	}

	public function getPosition():Vector4
	{
		if (position == null) position = new Vector4();
		return position;
	}

	public function setPosition(value:Vector4):Vector4
	{
		if (position == null) position = new Vector4();
		position.setTo(value.x, value.y, value.z);

		if (source != null)
		{
			anglesArray[0] = Math.PI / 6;
			anglesArray[1] = -Math.PI / 6;
			AL.sourcefv(source, AL.STEREO_ANGLES, anglesArray);

			if (AudioManager.__spatializeSupported
				&& (Math.abs(position.x) > 1e-04 || Math.abs(position.y) > 1e-04 || Math.abs(position.z) > 1e-04))
			{
				AL.sourcei(source, AL.SOURCE_SPATIALIZE_SOFT, AL.TRUE);
			}
			else
			{
				AL.sourcei(source, AL.SOURCE_SPATIALIZE_SOFT, AL.FALSE);
			}

			AL.source3f(source, AL.POSITION, position.x, position.y, position.z);
		}

		return value;
	}

	function readToBufferData(data:ArrayBufferView, currentPCM:Int):Int
	{
		if (decoder.eof || currentPCM >= loopPoints[1])
		{
			if (streamEnded = loops <= streamLoops || !decoder.seek(loopPoints[0])) return 0;
			streamLoops++;
		}

		var total = 0, len:Int;
		while (!(streamEnded = decoder.eof))
		{
			if ((len = (loopPoints[1] - currentPCM) * parent.buffer.channels * (parent.buffer.bitsPerSample >> 3)) <= (currentPCM = bufferLen - total))
			{
				total += decoder.decode(data.buffer, total, len);
				if (loops > streamLoops)
				{
					decoder.seek(currentPCM = loopPoints[0]);
					streamLoops++;
				}
				else
				{
					streamEnded = true;
					break;
				}
			}
			else
			{
				return total += decoder.decode(data.buffer, total, currentPCM);
			}
		}
		return total;
	}

	function fillBuffers(n:Int):Void
	{
		var max = STREAM_MAX_BUFFERS - 1;
		var i:Int, j:Int, data:ArrayBufferView, pcm:Int, decoded:Int;
		while (n-- > 0 && !streamEnded)
		{
			data = bufferViews[(i = max - filledBuffers) > 0 ? i : 0];
			pcm = Int64.toInt(decoder.tell());
			decoded = readToBufferData(data, pcm);
			if (decoded <= 0) break;
			else if (filledBuffers < STREAM_MAX_BUFFERS) filledBuffers++;

			seekMutex.acquire();

			j = i;
			while (i < max)
			{
				bufferViews[i] = bufferViews[++j];
				bufferCurs[i] = bufferCurs[j];
				bufferLens[i] = bufferLens[j];
				i = j;
			}
			bufferViews[max] = data;
			bufferCurs[max] = pauseSample = pcm;
			bufferLens[max] = decoded;
			queuedBuffers++;

			seekMutex.release();
		}
	}

	function flushBuffers():Void
	{
		seekMutex.acquire();

		var i = STREAM_MAX_BUFFERS - queuedBuffers + internalQueuedBuffers;
		while (internalQueuedBuffers < STREAM_FLUSH_BUFFERS && internalQueuedBuffers < queuedBuffers)
		{
			AL.bufferData(buffers[nextBuffer], format, bufferViews[i], bufferLens[i], parent.buffer.sampleRate);
			AL.sourceQueueBuffer(source, buffers[nextBuffer]);
			if (++nextBuffer == STREAM_FLUSH_BUFFERS) nextBuffer = 0;
			internalQueuedBuffers++;
			i++;
		}

		seekMutex.release();
	}

	function skipBuffers(n:Int):Void
	{
		seekMutex.acquire();

		internalQueuedBuffers -= (n = AL.sourceUnqueueBuffers(source, n).length);
		queuedBuffers -= n;

		seekMutex.release();
	}

	function snapBuffersToSample(sample:Int, force:Bool, n:Int):Void
	{
		if (!force)
		{
			for (i in (STREAM_MAX_BUFFERS - queuedBuffers)...(STREAM_MAX_BUFFERS - STREAM_MIN_BUFFERS))
				if (sample >= bufferCurs[i] && sample < bufferCurs[i] + (bufferLens[i] / (parent.buffer.bitsPerSample >> 3) / parent.buffer.channels))
			{
				skipBuffers(i - STREAM_MAX_BUFFERS + queuedBuffers);
				AL.sourcei(source, AL.SAMPLE_OFFSET, sample - bufferCurs[i]);
				return;
			}
		}

		AL.sourceUnqueueBuffers(source, AL.getSourcei(source, AL.BUFFERS_QUEUED));

		internalQueuedBuffers = queuedBuffers = filledBuffers = streamLoops = nextBuffer = 0;
		decoder.seek(sample);
		fillBuffers(n);
		flushBuffers();
	}

	static function streamThreadRun():Void
	{
		var i:Int, backend:NativeAudioSource, process:Int, v:Int;
		while (true)
		{
			streamMutex.acquire();
			if ((i = queuedStreamAudios.length) == 0 && streamAudios.length == 0)
			{
				streamMutex.release();
				break;
			}

			try {
				while (i-- > 0)
				{
					(backend = queuedStreamAudios[i]).streaming = true;
					backend.pending = false;
					streamAudios.push(backend);
				}
				queuedStreamAudios.resize(0);

				i = streamAudios.length;
				while (i-- > 0)
				{
					backend = streamAudios[i];
					if (!backend.streaming || backend.source == null) backend.removeStream();
					else if (backend.mutex.tryAcquire())
					{
						backend.skipBuffers(AL.getSourcei(backend.source, AL.BUFFERS_PROCESSED));

						process = backend.queuedBuffers < STREAM_MIN_BUFFERS ? STREAM_MIN_BUFFERS - backend.queuedBuffers : 0;
						process = STREAM_PROCESS_BUFFERS > process ? STREAM_PROCESS_BUFFERS : process;
						if ((process = (v = STREAM_USABLE_BUFFERS - backend.queuedBuffers) > process ? process : v) > 0) backend.fillBuffers(process);

						backend.flushBuffers();

						if (AL.getSourcei(backend.source, AL.SOURCE_STATE) == AL.STOPPED)
						{
							AL.sourcePlay(backend.source);
							backend.timer = resetTimer(backend.timer, (backend.loopPoints[1] - backend.bufferCurs[STREAM_MAX_BUFFERS - backend.queuedBuffers])
								* 1000.0 / backend.parent.buffer.sampleRate / backend.getPitch(), backend.complete);
						}

						backend.mutex.release();
						if (backend.streamEnded && backend.queuedBuffers == backend.internalQueuedBuffers) backend.removeStream();
					}
				}
			}
			catch (e:haxe.Exception)
			{
				trace(e);
				Sys.println(e.details());
			}
			streamMutex.release();
			Sys.sleep(STREAM_PROCESS_DELAY);
		}

		threadRunning = false;
	}

	inline function removeStream():Void
	{
		pending = streaming = false;
		streamAudios.remove(this);
	}

	inline function stopStream():Void
	{
		if (!pending)
		{
			if (streaming)
			{
				pending = true;
				streaming = false;
			}
			else
			{
				streamMutex.acquire();
				queuedStreamAudios.remove(this);
				streamMutex.release();
			}
		}
	}

	inline function resetStream():Void
	{
		if (pending && (streaming = pending)) pending = false;

		if (!pending && !streaming)
		{
			pending = true;

			streamMutex.acquire();
			queuedStreamAudios.push(this);
			streamMutex.release();
		}

		if (!threadRunning || streamThread == null)
		{
			streamThread = Thread.create(streamThreadRun);
			threadRunning = true;
		}
	}

	public function getPeaks(offsetMs:Float):Array<Float>
	{
		if (peaks == null) peaks = [];
		if (!playing)
		{
			for (i in 0...peaks.length) peaks[i] = 0;
			return peaks;
		}
		if (parent.buffer.channels != peaks.length) peaks.resize(parent.buffer.channels);

		inline function nothing()
		{
			if (streamed) mutex.release();
			for (i in 0...parent.buffer.channels) peaks[i] = 0;
			return peaks;
		}

		var i = 0, buffer:ArrayBuffer, bufferLen:Int;
		if (streamed)
		{
			mutex.acquire();
			if (filledBuffers == 0) return nothing();

			i = STREAM_MAX_BUFFERS - queuedBuffers;
			buffer = bufferViews[i].buffer;
			bufferLen = bufferLens[i];
		}
		else
		{
			buffer = parent.buffer.data.buffer;
			bufferLen = buffer.length;
		}

		var byteSize = 1 << parent.buffer.bitsPerSample;
		var wordSize = parent.buffer.bitsPerSample >> 3;
		var samplesToDo = parent.buffer.sampleRate >> 4;
		var pos = (AL.getSourcei(source, AL.SAMPLE_OFFSET) + Std.int(offsetMs / 1000.0 * parent.buffer.sampleRate) - samplesToDo);
		pos *= parent.buffer.channels * wordSize;

		if (pos < 0)
		{
			if (samplesToDo < pos) return nothing();
			samplesToDo -= pos;
			do {
				if (i == 0) pos = 0;
				else
				{
					buffer = bufferViews[--i].buffer;
					bufferLen = bufferLens[i];
					pos += bufferLen;
				}
			} while (pos < 0);
		}
		else if (pos >= bufferLen)
		{
			if (!streamed) return nothing();
			do {
				if (++i >= bufferLens.length) return nothing();

				pos -= bufferLen;
				buffer = bufferViews[i].buffer;
				bufferLen = bufferLens[i];
			} while (pos >= bufferLen);
		}

		if (mins == null)
		{
			mins = [for (i in 0...parent.buffer.channels) -0x7FFFFFFF];
			maxs = [for (i in 0...parent.buffer.channels) -0x7FFFFFFF];
		}
		else
		{
			for (i in 0...parent.buffer.channels) maxs[i] = mins[i] = -0x7FFFFFFF;
		}

		var c = 0, b:Int;
		while (samplesToDo > 0) {
			if (wordSize == 2) b = ArrayBufferIO.getInt16(buffer, pos);
			else if (wordSize == 3)
			{
				b = ArrayBufferIO.getUint16(buffer, pos) | (buffer.get(pos + 2) << 16);
				if (b & 0x800000 != 0) b -= 0x1000000;
			}
			else if (wordSize == 4) b = ArrayBufferIO.getInt32(buffer, pos);
			else b = ArrayBufferIO.getUint8(buffer, pos) - 128;

			((b > maxs[c]) ? (maxs[c] = b) : (if (-b > mins[c]) (mins[c] = -b)));
			if ((pos += wordSize) >= bufferLen)
			{
				if (!streamed || ++i >= bufferLens.length) break;
				pos = 0;
				buffer = bufferViews[i].buffer;
				bufferLen = bufferLens[i];
			}
			else if (++c == parent.buffer.channels)
			{
				c = 0;
				samplesToDo--;
			}
		}

		if (streamed) mutex.release();
		for (i in 0...parent.buffer.channels) peaks[i] = (maxs[i] + mins[i]) / byteSize;
		return peaks;
	}
}