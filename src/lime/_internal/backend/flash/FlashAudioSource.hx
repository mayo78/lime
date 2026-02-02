package lime._internal.backend.flash;

import lime.math.Vector4;
import lime.media.AudioSource;
#if flash
import flash.media.SoundChannel;
import flash.media.SoundTransform;
import flash.media.Sound;
#end

@:access(lime.media.AudioBuffer)
class FlashAudioSource
{
	public var parent:AudioSource;

	private var completed:Bool;
	private var length:Float;
	private var loopTime:Float;
	private var loops:Int;
	private var pauseTime:Float;
	private var playing:Bool;
	private var position:Vector4;
	#if flash
	public var channel:SoundChannel;
	public var sound:Sound;

	private var soundTransform:SoundTransform;
	private var timerID:Int;
	#end

	public function new(parent:AudioSource)
	{
		this.parent = parent;
		#if flash
		timerId = -1;
		#end
		init();
	}

	private function init():Void
	{
		#if flash
		if (soundTransform == null) soundTransform = new SoundTransform(1, 0);
		timerId = -1;
		#end
	}

	public function dispose():Void
	{
		soundTransform = null;
	}

	public function load():Void
	{
		#if flash
		init();
		if (parent.buffer != null) sound = parent.buffer.__srcSound;
		if (sound != null) length = sound.length;
		#end
	}

	public function unload():Void
	{
		#if flash
		sound = null;
		#end
		length = 0;
		loopTime = 0;
		pauseTime = 0;
	}

	public function play():Void
	{
		#if flash
		if (sound == null || playing) return;

		playing = true;
		completed = false;

		if (channel != null) channel.stop();
		channel = sound.play(pauseTime + parent.offset, 0, soundTransform);

		resetTimer(Std.int(length - pauseTime - parent.offset));
		#end
	}

	public function pause():Void
	{
		playing = false;

		#if flash
		if (channel != null)
		{
			pauseTime = channel.position;
			channel.stop();
			channel = null;
		}
		else
		{
			pauseTime = 0;
		}
		#end
	}

	public function stop():Void
	{
		playing = false;
		pauseTime = 0;

		#if flash
		if (channel != null)
		{
			channel.stop();
			channel = null;
		}
		#end
	}

	// Event Handlers
	private inline function stopTimer():Void
	{
		#if flash
		if (timerID != -1)
		{
			untyped __global__["flash.utils.clearInterval"](timerID);
			timerID = -1;
		}
		#end
	}

	private inline function resetTimer(ms:Int):Void
	{
		#if flash
		stopTimer();

		var me = this;
		timerID = untyped __global__["flash.utils.setInterval"](function() me.complete(), ms);
		#end
	}

	private function complete()
	{
		#if flash
		if (channel != null)
		{
			channel.stop();
			channel = null;
		}

		if (loops > 0)
		{
			loops--;
			pauseTime = loopTime;
			playing = false;
			play();
		}
		else
		{
			stopTimer();
			completed = true;
			pauseTime = 0;
		}

		parent.onComplete.dispatch();
		#end
	}

	// Get & Set Methods
	public function getCurrentTime():Float
	{
		#if flash
		if (completed)
		{
			return length - parent.offset;
		}
		else if (channel != null && playing)
		{
			return channel.position - parent.offset;
		}
		#end

		return pauseTime - parent.offset;
	}

	public function setCurrentTime(value:Float):Float
	{
		pauseTime = value + parent.offset;
		if (pauseTime < 0) pauseTime = 0;

		#if flash
		if (playing)
		{
			playing = false;
			play();
		}
		#end

		return value;
	}

	public function getGain():Float
	{
		#if flash
		if (soundTransform != null) return soundTransform.volume;
		#end
		return 1;
	}

	public function setGain(value:Float):Float
	{
		#if flash
		if (soundTransform != null)
		{
			soundTransform.volume = value;
			if (channel != null) channel.soundTransform = soundTransform;
		}
		#end
		return value;
	}

	public function getLatency():Float
	{
		return 0;
	}

	public function getLength():Float
	{
		if (length <= parent.offset) return 0;
		return length - parent.offset;
	}

	public function setLength(value:Float):Float
	{
		length = value + parent.offset;
		#if flash
		if (sound != null)
		{
			if (length <= 0 || length >= sound.length) length = sound.length;
			if (playing) resetTimer();
		}
		#end

		if (value < 0) return 0;
		return value;
	}

	public function getLoops():Int
	{
		return loops;
	}

	public function setLoops(value:Int):Int
	{
		return loops = value;
	}

	public function getLoopTime():Float
	{
		if (loopTime <= parent.offset) return 0;
		return loopTime - parent.offset;
	}

	public function setLoopTime(value:Float):Float
	{
		loopTime = value + parent.offset;
		return value;
	}

	public function getPan():Float
	{
		#if flash
		if (soundTransform != null) return soundTransform.pan;
		#end
		return 0;
	}

	public function setPan(value:Float):Float
	{
		position.setTo(value, 0, -Math.sqrt(1 - value * value));
		#if flash
		if (soundTransform != null)
		{
			soundTransform.pan = value;
			if (channel != null) channel.soundTransform = soundTransform;
		}
		#end
		return value;
	}

	public function getPitch():Float
	{
		lime.utils.Log.verbose("AudioSource.pitch is not supported in Flash.");
		return 1;
	}

	public function setPitch(value:Float):Float
	{
		return inline getPitch();
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

		#if flash
		if (soundTransform != null)
		{
			soundTransform.pan = value.x;
			if (channel != null) channel.soundTransform = soundTransform;
		}
		#end

		return position;
	}
}
