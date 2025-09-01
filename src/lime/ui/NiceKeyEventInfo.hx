package lime.ui;

import lime._internal.backend.native.NativeApplication;

class NiceKeyEventInfo
{
	public var type:KeyEventType;
	public var keyCode:KeyCode;
	public var modifier:KeyModifier;
	public var timestamp:haxe.Int64;
	public var repeat:Bool;

	public function new() {}
}