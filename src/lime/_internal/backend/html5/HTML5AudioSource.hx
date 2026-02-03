package lime._internal.backend.html5;

import lime.math.Vector4;
import lime.media.AudioSource;
#if lime_howlerjs
import js.html.audio.AnalyserNode;
import js.html.audio.ChannelSplitterNode;
import js.html.MediaElement;
import js.lib.Float32Array;
import lime.media.howlerjs.Howl;
import lime.media.howlerjs.Howler;
#end

@:access(lime.media.AudioBuffer)
class HTML5AudioSource
{
	public var parent:AudioSource;

	private var completed:Bool;
	private var gain:Float;
	private var length:Float;
	private var loopTime:Float;
	private var loops:Int;
	private var pan:Float;
	private var pauseTime:Float;
	private var peaks:Array<Float>;
	private var pitch:Float;
	private var position:Vector4;
	#if lime_howlerjs
	public var id:Int;
	public var howl:Howl;

	private var analyserLeft:AnalyserNode;
	private var analyserRight:AnalyserNode;
	private var channelSplitter:ChannelSplitterNode;
	private var dataArrayLeft:Float32Array;
	private var dataArrayRight:Float32Array;
	private var mins:Array<Float>;
	private var maxs:Array<Float>;
	private var timerID:Int;
	#end

	public function new(parent:AudioSource)
	{
		this.parent = parent;
		gain = 1;
		pan = 0;
		pitch = 1;
		#if lime_howlerjs
		id = -1;
		timerID = -1;
		#end
	}

	public function dispose():Void {}

	public function load():Void
	{
		#if lime_howlerjs
		if (parent.buffer != null) howl = parent.buffer.__srcHowl;
		if (howl != null) length = howl.duration() * 1000;
		#end
	}

	public function unload():Void
	{
		// Howl sounds are automatically unloaded if it has stopped.
		#if lime_howlerjs
		if (channelSplitter != null) channelSplitter.disconnect();
		if (analyserLeft != null) analyserLeft.disconnect();
		if (analyserRight != null) analyserRight.disconnect();
		channelSplitter = null;
		analyserLeft = null;
		analyserRight = null;
		howl = null;
		id = -1;
		#end
		length = 0;
		loopTime = 0;
		pauseTime = 0;
	}

	public function play():Void
	{
		#if lime_howlerjs
		if (howl == null || (id != -1 && howl.playing(id))) return;

		completed = false;

		var pos = (pauseTime + parent.offset) / 1000;
		if (id == -1)
		{
			id = howl.play();
			updateLoop();
			howl.volume(gain, id);
			howl.seek(pos, id);

			// nvm, still causes muffling somehow, disable position entirely.
			// https://github.com/goldfire/howler.js/issues/112
			//howl.pannerAttr({distanceModel: "equalpower"}, id);	
		}
		else
		{
			updateLoop();
			howl.volume(gain, id);
			howl.seek(pos, id);
			howl.play(id);
		}

		resetTimer(Std.int((length - pauseTime - parent.offset) / howl.rate(id)));
		#end
	}

	public function pause():Void
	{
		#if lime_howlerjs
		if (howl != null && id != -1)
		{
			pauseTime = howl.seek(id) * 1000;
			howl.pause(id);
		}
		else
		{
			pauseTime = 0;
		}
		stopTimer();
		#end
	}

	public function stop():Void
	{
		pauseTime = 0;

		#if lime_howlerjs
		if (howl != null && id != -1)
		{
			howl.stop(id);
		}
		stopTimer();
		#end
	}

	// Event Handlers
	private inline function stopTimer():Void
	{
		#if lime_howlerjs
		if (timerID != -1)
		{
			untyped clearInterval(timerID);
			timerID = -1;
		}
		#end
	}

	private inline function resetTimer(ms:Int):Void
	{
		#if lime_howlerjs
		stopTimer();

		var me = this;
		timerID = untyped setInterval(function() me.complete(), ms);
		#end
	}

	private function complete()
	{
		#if lime_howlerjs
		howl.stop(id);

		if (loops > 0)
		{
			var wasLooping = howl.loop(id);
			loops--;
			updateLoop();
			if (!wasLooping)
			{
				howl.seek((loopTime + parent.offset) / 1000, id);
				howl.play(id);
			}
			pauseTime = loopTime;
			resetTimer(Std.int((length - loopTime - parent.offset) / howl.rate(id)));
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
		#if lime_howlerjs
		if (completed)
		{
			return length - parent.offset;
		}
		else if (howl != null && id != -1)
		{
			return howl.seek(id) * 1000 - parent.offset;
		}
		#end

		return pauseTime - parent.offset;
	}

	public function setCurrentTime(value:Float):Float
	{
		pauseTime = value + parent.offset;

		#if lime_howlerjs
		if (howl != null && id != -1)
		{
			if (pauseTime < 0 || !Math.isFinite(pauseTime)) pauseTime = 0;
			else if (pauseTime > length) pauseTime = length;
			howl.seek(pauseTime / 1000, id);
		}
		#end

		return value;
	}

	public function getGain():Float
	{
		return gain;
	}

	public function setGain(value:Float):Float
	{
		#if lime_howlerjs
		if (howl != null && id != -1)
		{
			howl.volume(value, id);
		}
		#end
		return gain = value;
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

		#if lime_howlerjs
		if (howl != null)
		{
			var duration = howl.duration() * 1000;
			if (length <= 0 || length >= duration) length = duration;
			if (id != -1 && howl.playing(id)) resetTimer(Std.int((length - howl.seek(id) * 1000) / howl.rate(id)));
		}
		#end
		updateLoop();

		return value;
	}

	public function getLoopTime():Float
	{
		if (loopTime <= parent.offset) return 0;
		return loopTime - parent.offset;
	}

	public function setLoopTime(value:Float):Float
	{
		loopTime = value + parent.offset;

		#if lime_howlerjs
		if (howl != null)
		{
			if (loopTime < 0) loopTime = 0;
			else
			{
				var duration = howl.duration() * 1000;
				if (loopTime >= duration) loopTime = duration;
			}
		}
		#end
		updateLoop();
		return value;
	}

	public function getLoops():Int
	{
		return loops;
	}

	public function setLoops(value:Int):Int
	{
		loops = value;
		updateLoop();
		return value;
	}

	private function updateLoop()
	{
		#if lime_howlerjs
		if (howl != null && id != -1)
		{
			var duration = howl.duration() * 1000;
			howl.loop(loops > 0 && loopTime <= 0 && length >= duration, id);
		}
		#end
	}

	public function getPan():Float
	{
		return pan;
	}

	public function setPan(value:Float):Float
	{
		#if lime_howlerjs
		if (howl != null && id != -1)
		{
			position.setTo(value, 0, -Math.sqrt(1 - value * value));
			//howl.pos(0, 0, 0, id);
			howl.stereo(value, id);
		}
		#end
		return pan = value;
	}

	public function getPitch():Float
	{
		return pitch;
	}

	public function setPitch(value:Float):Float
	{
		#if lime_howlerjs
		if (howl != null && id != -1)
		{
			howl.rate(value, id);
			resetTimer(Std.int((length - howl.seek(id) * 1000) / howl.rate(id)));
		}
		#end
		return pitch = value;
	}

	public function getPlaying():Bool
	{
		#if lime_howlerjs
		if (howl != null && id != -1) return howl.playing(id);
		#end
		return false;
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

		/*#if lime_howlerjs
		if (howl != null && id != -1)
		{
			howl.pos(position.x, position.y, position.z, id);
		}
		#end*/

		return position;
	}

	public function getPeaks(offsetMs:Float):Array<Float>
	{
		if (peaks == null) peaks = [0, 0];

		#if lime_howlerjs
		if (howl == null || id == -1 || !howl.playing(id))
		{		
			for (i in 0...2) peaks[i] = 0;
			return peaks;
		}

		if (channelSplitter == null)
		{
			var node = untyped howl._soundById(id)._node;

			//html5 audios is extremely buggy.
			/*
			var isHTML5 = (node is MediaElement);
			if (isHTML5) node = Howler.ctx.createMediaElementSource(untyped node);
			*/
			if ((node is MediaElement))
			{
				for (i in 0...2) peaks[i] = 0;
				return peaks;
			}

			var ctx = untyped node.context;
			if (untyped node.bufferSource)
			{
				node = untyped node.bufferSource;
			}

			channelSplitter = new ChannelSplitterNode(untyped ctx, {numberOfOutputs: 2});
			analyserLeft = new AnalyserNode(untyped ctx);
			analyserRight = new AnalyserNode(untyped ctx);
			analyserLeft.fftSize = analyserRight.fftSize = 2048;
			analyserLeft.maxDecibels = analyserRight.maxDecibels = 0;
			analyserLeft.minDecibels = analyserRight.minDecibels = -120;

			untyped node.connect(channelSplitter);
			channelSplitter.connect(analyserLeft, 0);
			channelSplitter.connect(analyserRight, 1);

			//if (isHTML5) channelSplitter.connect(untyped Howler.ctx.destination);
		}

		if (dataArrayLeft == null) dataArrayLeft = new Float32Array(2048);
		if (dataArrayRight == null) dataArrayRight = new Float32Array(2048);

		analyserLeft.getFloatTimeDomainData(dataArrayLeft);
		analyserRight.getFloatTimeDomainData(dataArrayRight);

		if (mins == null)
		{
			mins = [for (i in 0...2) -1];
			maxs = [for (i in 0...2) -1];
		}
		else
		{
			for (i in 0...2) maxs[i] = mins[i] = -1;
		}

		for (v in dataArrayLeft) ((v > maxs[0]) ? (maxs[0] = v) : (if (-v > mins[0]) (mins[0] = -v)));
		for (v in dataArrayRight) ((v > maxs[1]) ? (maxs[1] = v) : (if (-v > mins[1]) (mins[1] = -v)));

		for (i in 0...2) peaks[i] = (maxs[i] + mins[i]) * 0.5;
		#end
		return peaks;
	}
}
