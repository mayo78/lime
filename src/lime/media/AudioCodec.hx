package lime.media;

#if (haxe_ver >= 4.0) enum #else @:enum #end abstract AudioCodec(Null<String>) from Null<String> to Null<String>
{
	var WAVE = "WAVE";
	var MP3 = "MP3";
	var OGG = "OGG";
	var FLAC = "FLAC";

	public static function fromHTML5(value:String):AudioCodec
	{
		return switch (value)
		{
			case "audio/wav": WAVE;
			case "audio/mp3", "audio/mpeg": MP3;
			//case "audio/mp4": MP4;
			//case "audio/aac", "audio/aacp": AAC;
			case "audio/ogg": OGG;
			//case "audio/webm": WEBM;
			//case "audio/x-caf": X_CAF;
			case "audio/flac": FLAC;
			default: null;
		}
	}

	public function toHTML5():Null<String>
	{
		return switch (cast this : AudioCodec)
		{
			case WAVE: "audio/wav";
			case MP3: "audio/mpeg";
			case OGG: "audio/ogg";
			case FLAC: "audio/flac";
			default: null;
		}
	}
}
