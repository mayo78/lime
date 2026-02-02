package lime.media;

import lime.app.Event;
import lime.media.openal.AL;
import lime.media.openal.ALSource;
import lime.math.Vector4;

#if !lime_debug
@:fileXml('tags="haxe,release"')
@:noDebug
#end
/**
	The `AudioSource` class provides a way to control audio playback in a Lime application. 
	It allows for playing, pausing, and stopping audio, as well as controlling various 
	audio properties such as gain, pitch, and looping.

	Depending on the platform, the audio backend may vary, but the API remains consistent.

	@see lime.media.AudioBuffer
**/
class AudioSource
{
	/**
		An event that is dispatched when this audio playback have completed or looped.
	**/
	public var onComplete = new Event<Void->Void>();
	
	/**
		The `AudioBuffer` associated with this `AudioSource`.
	**/
	public var buffer:AudioBuffer;

	/**
		The current playback position of the audio, in milliseconds.
	**/
	public var currentTime(get, set):Float;

	/**
		The gain (volume) of the audio. A value of `1.0` represents the default volume.
		Property is in a linear scale.
	**/
	public var gain(get, set):Float;

	/**
		The current latency of this 'AudioSource'.
	**/
	public var latency(get, never):Float;

	/**
		The length of the audio, in milliseconds.
		Setting this to 0 will set back to the original length.
	**/
	public var length(get, set):Float;

	/**
		In which audio playback time the audio will loop.
	**/
	public var loopTime(get, set):Float;

	/**
		The number of times the audio will loop. A value of `0` means the audio will not loop.
	**/
	public var loops(get, set):Int;

	/**
		The offset within the audio buffer to start playback, in milliseconds.
		NOTE: The original documentation said it is in samples, but its actually in milliseconds.
	**/
	public var offset:Float;

	/**
		The stereo pan of the audio source.
		Setting this will set the position back to default.
	**/
	public var pan(get, set):Float;

	/**
		The pitch of the audio. A value of `1.0` represents the default pitch.
	**/
	public var pitch(get, set):Float;

	/**
		An property if this 'AudioSource' is playing.
	**/
	public var playing(get, never):Bool;

	/**
		The 3D position of the audio source, represented as a `Vector4`.
		Setting this will set the pan back to default.
	**/
	public var position(get, set):Vector4;

	/**
		The current used cloned decoder to be used and played.
	**/
	public var decoder(default, null):Null<AudioDecoder>;

	/**
		An indicator if an decoder was cloned.
	**/
	public var standaloneDecoder(default, null):Bool;

	@:noCompletion private var __backend:AudioSourceBackend;

	/**
		Creates a new `AudioSource` instance.
		@param buffer The `AudioBuffer` to associate with this `AudioSource`.
		@param offset The starting offset within the audio buffer, in samples.
		@param length The length of the audio to play, in milliseconds. If `null`, the full buffer is used.
		@param loops The number of times to loop the audio. `0` means no looping.
	**/
	public function new(buffer:AudioBuffer = null, offset:Float = 0, length:Null<Int> = null, loops:Int = 0)
	{
		__backend = new AudioSourceBackend(this);

		this.buffer = buffer;
		this.offset = offset;
		if (length != null && length != 0) this.length = length;
		this.loops = loops;

		peaks = [];

		if (buffer != null) __backend.load();
	}

	/**
		Releases any resources used by this `AudioSource`.
	**/
	public function dispose():Void
	{
		__backend.stop();
		__backend.unload();
		__backend.dispose();
	}

	/**
		Loads the buffer to this 'AudioSource'.
	**/
	public function load():Void
	{
		__backend.unload();
		__backend.load();
	}

	/**
		Unloads the current loaded buffer from this 'AudioSource'.
	**/
	public function unload():Void
	{
		__backend.stop();
		__backend.unload();
	}

	/**
		Starts or resumes audio playback.
	**/
	public function play():Void
	{
		__backend.play();
	}

	/**
		Pauses audio playback.
	**/
	public function pause():Void
	{
		__backend.pause();
	}

	/**
		Stops audio playback and resets the playback position to the beginning.
	**/
	public function stop():Void
	{
		__backend.stop();
	}

	@:noCompletion private inline function init():Void
	{
		__backend.load();
	}

	// Get & Set Methods
	@:noCompletion private inline function get_currentTime():Float
	{
		return __backend.getCurrentTime();
	}

	@:noCompletion private inline function set_currentTime(value:Float):Float
	{
		return __backend.setCurrentTime(value);
	}

	@:noCompletion private inline function get_gain():Float
	{
		return __backend.getGain();
	}

	@:noCompletion private inline function set_gain(value:Float):Float
	{
		return __backend.setGain(value);
	}

	@:noCompletion private inline function get_latency():Float
	{
		return __backend.getLatency();
	}

	@:noCompletion private inline function get_length():Float
	{
		return __backend.getLength();
	}

	@:noCompletion private inline function set_length(value:Float):Float
	{
		return __backend.setLength(value);
	}

	@:noCompletion private inline function get_loopTime():Float
	{
		return __backend.getLoopTime();
	}

	@:noCompletion private inline function set_loopTime(value:Float):Float
	{
		return __backend.setLoopTime(value);
	}

	@:noCompletion private inline function get_loops():Int
	{
		return __backend.getLoops();
	}

	@:noCompletion private inline function set_loops(value:Int):Int
	{
		return __backend.setLoops(value);
	}

	@:noCompletion private inline function get_pan():Float
	{
		return __backend.getPan();
	}

	@:noCompletion private inline function set_pan(value:Float):Float
	{
		return __backend.setPan(value);
	}

	@:noCompletion private inline function get_pitch():Float
	{
		return __backend.getPitch();
	}

	@:noCompletion private inline function set_pitch(value:Float):Float
	{
		return __backend.setPitch(value);
	}

	@:noCompletion private inline function get_playing():Bool
	{
		return __backend.getPlaying();
	}

	@:noCompletion private inline function get_position():Vector4
	{
		return __backend.getPosition();
	}

	@:noCompletion private inline function set_position(value:Vector4):Vector4
	{
		return __backend.setPosition(value);
	}
}

#if lime_openal
@:noCompletion private typedef AudioSourceBackend = lime._internal.backend.native.NativeAudioSource;
#elseif (js && html5)
@:noCompletion private typedef AudioSourceBackend = lime._internal.backend.html5.HTML5AudioSource;
#elseif flash
@:noCompletion private typedef AudioSourceBackend = lime._internal.backend.flash.FlashAudioSource;
#end