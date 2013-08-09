package {

import flash.display.Sprite;
import flash.display.StageScaleMode;
import flash.display.StageAlign;
import flash.display.Bitmap;
import flash.display.Loader;
import flash.media.Video;
import flash.net.NetConnection;
import flash.net.NetStream;
import flash.net.ObjectEncoding;
import flash.net.URLRequest;
import flash.events.Event;
import flash.events.NetStatusEvent;
import flash.events.MouseEvent;
import flash.utils.setInterval;
import flash.utils.clearInterval;
import flash.geom.Rectangle;

// TEST
//import flash.display.BitmapData;
//import flash.display.Bitmap;

[SWF(backgroundColor=0)]
public class BasicPlayer extends Sprite
{
    private var conn : NetConnection;
    private var stream : NetStream;
    private var video : Video;

    private var buttons_visible : Boolean;

    private var shadow_opacity : Number;
    private var grey_shadow : Sprite;

    [Embed (source="play.png")]
    private var PlayButton : Class;
    [Embed (source="play_hover.png")]
    private var PlayButtonHover : Class;
    private var play_button_bitmap : Bitmap;
    private var play_button_hover_bitmap : Bitmap;
    private var play_button : Sprite;

    // Toggles fullscreen mode.
    [Embed (source="fullscreen.png")]
    private var FullscreenButton : Class;
    private var fullscreen_button : Sprite;

    // Toggles horizontal mode. In normal mode, video is scaled in such a way
    // that it fits the screen both vertically in horizontally. In horizontal
    // mode, only horizontal size is considered. This is useful for cutting
    // artificial black fields on top and on bottom of the video for wide
    // screens.
    [Embed (source="horizontal.png")]
    private var HorizontalButton : Class;
    private var horizontal_button : Sprite;

    // If true, then horizontal mode is enabled.
    private var horizontal_mode : Boolean;

    // Stage width and height stored to workaround incorrect stage size bug
    // reported by flash.
    private var stage_width : int;
    private var stage_height : int;

    private var channel_uri : String;
    private var stream_name : String;

    private var reconnect_interval : uint;
    private var reconnect_timer : uint;

    private var play_duration : uint;
    private var play_timer : uint;

    private var hide_buttons_timer : uint;

    private var playing : Boolean;
    private var reconnecting : Boolean;

    private var buffer_time : Number;
    private var start_paused : Boolean;

    private var buffering_complete : Boolean;
    private var frame_no : uint;

    private function doResize () : void
    {
        stage_width  = stage.stageWidth;
        stage_height = stage.stageHeight;

	grey_shadow.width = stage_width;
	grey_shadow.height = stage_height;

        repositionButtons ();
        repositionVideo ();
    }

    private function videoShouldBeHorizontal () : Boolean
    {
	if (stage_width == 0 || stage_height == 0)
	    return true;

	var x_aspect : Number = (0.0 + Number (video.videoWidth))  / Number (stage_width);
	var y_aspect : Number = (0.0 + Number (video.videoHeight)) / Number (stage_height);

	return x_aspect >= y_aspect;
    }

    private function repositionVideo () : void
    {
	if (horizontal_mode || videoShouldBeHorizontal()) {
	    video.width = stage_width;
	    video.height = stage_width * (video.videoHeight / video.videoWidth);
	    video.x = 0;
	    video.y = (stage_height - video.height) / 2;
	} else {
	    video.width = stage_height * (video.videoWidth / video.videoHeight);
	    video.height = stage_height;
	    video.x = (stage_width - video.width) / 2;
	    video.y = 0;
	}
    }

    private function repositionButtons () : void
    {
	horizontal_button.visible = (buttons_visible && !videoShouldBeHorizontal());

	fullscreen_button.x = stage_width - fullscreen_button.width - 20;
	fullscreen_button.y = stage.stageHeight - fullscreen_button.height - 20;

	horizontal_button.x = stage_width - horizontal_button.width - 90;
	horizontal_button.y = stage.stageHeight - horizontal_button.height - 20;

	play_button.x = stage_width  / 2 - play_button.width  / 2;
	play_button.y = stage_height / 2 - play_button.height / 2;
    }

    private function repositionMessage (msg : Loader) : void
    {
	msg.x = (stage_width - msg.width) / 2;
	msg.y = (stage_height - msg.height) * (4.0 / 5.0);
    }

    private function showVideo () : void
    {
	video.visible = true;
//	showButtons ();
    }

    private function onStreamNetStatus (event : NetStatusEvent) : void
    {
	if (event.info.code == "NetStream.Buffer.Full") {
	    buffering_complete = true;

	    // Why resetting reconnect_interval?
	    reconnect_interval = 0;

	    repositionVideo ();
	    showVideo ();

// RtmpSampleAccess TEST
//            var bdata : BitmapData = new BitmapData (100, 100, false, 0x11ffff33);
//            bdata.draw (video);
//            var bitmap : Bitmap = new Bitmap (bdata);
//            bitmap.x = 20;
//            bitmap.y = 20;
//            addChild (bitmap);
//            bitmap.visible = true;
	}
    }

    private function onEnterFrame (event : Event) : void
    {
	if (buffering_complete) {
	    repositionVideo ();

	    if (frame_no == 100)
		video.removeEventListener (Event.ENTER_FRAME, onEnterFrame);

	    ++frame_no;
	    return;
	}

	++frame_no;

	repositionVideo ();
	repositionButtons ();
	showVideo ();
    }

    private function doReconnect () : void
    {
	doSetChannel (channel_uri, stream_name, true /* reconnect */);
    }

    private function reconnectTick () : void
    {
	doReconnect ();
    }

    private function playTimerTick () : void
    {
	playing = false;

	grey_shadow.visible = true;
	play_button.visible = true;

	doDisconnect ();
	hideButtons ();
    }

    private function onConnNetStatus (event : NetStatusEvent) : void
    {
	if (event.info.code == "NetConnection.Connect.Success") {
	    if (reconnect_timer) {
		clearInterval (reconnect_timer);
		reconnect_timer = 0;
	    }

	    stream = new NetStream (conn);
	    stream.bufferTime = buffer_time;

	    video.attachNetStream (stream);

	    stream.addEventListener (NetStatusEvent.NET_STATUS, onStreamNetStatus);

	    buffering_complete = false;
	    frame_no = 0;
	    video.addEventListener (Event.ENTER_FRAME, onEnterFrame);

//	    stream.play (stream_name + "?paused");
	    stream.play (stream_name);
	    if (playing && reconnecting)
		stream.resume ();
	} else
	if (event.info.code == "NetConnection.Connect.Closed") {
	    video.removeEventListener (Event.ENTER_FRAME, onEnterFrame);

	    if (!reconnect_timer) {
		if (reconnect_interval == 0) {
		    doReconnect ();
		    return;
		}

		reconnect_timer = setInterval (reconnectTick, reconnect_interval);
	    }
	}
    }

    private function doDisconnect () : void
    {
	video.removeEventListener (Event.ENTER_FRAME, onEnterFrame);

	if (stream)
	    stream.removeEventListener (NetStatusEvent.NET_STATUS, onStreamNetStatus);

	if (conn != null) {
	    conn.removeEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);
	    conn.close ();
	    conn = null;
	}
    }

    public function setChannel (uri : String, stream_name_ : String) : void
    {
	doSetChannel (uri, stream_name_, false /* reconnect */);
    }

    private function doSetChannel (uri : String, stream_name_ : String, reconnect : Boolean) : void
    {
	video.removeEventListener (Event.ENTER_FRAME, onEnterFrame);

	if (!reconnect) {
	    if (play_timer) {
		clearInterval (play_timer);
		play_timer = 0;
	    }
	}

	if (!reconnect) {
	    reconnect_interval = 0;
	} else {
	    if (!reconnect_interval)
		reconnect_interval = 3000;
	}

	if (reconnect_timer) {
	    clearInterval (reconnect_timer);
	}
	reconnect_timer = setInterval (reconnectTick, reconnect ? reconnect_interval : 3000);

	channel_uri = uri;
	stream_name = stream_name_;

	doDisconnect ();

	conn = new NetConnection();
	conn.objectEncoding = ObjectEncoding.AMF0;

	conn.addEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);

	reconnecting = reconnect;
	conn.connect (uri);
    }

    private function toggleFullscreen (event : MouseEvent) : void
    {
        if (stage.displayState == "fullScreen")
          stage.displayState = "normal";
        else
          stage.displayState = "fullScreen";
    }

    private function toggleHorizontal (event : MouseEvent) : void
    {
	horizontal_mode = !horizontal_mode;
	repositionVideo ();
    }

    private function playButtonMouseOver (event : MouseEvent) : void
    {
	play_button_hover_bitmap.visible = true;
	play_button_bitmap.visible = false;
    }

    private function playButtonMouseOut (event : MouseEvent) : void
    {
	play_button_bitmap.visible = true;
	play_button_hover_bitmap.visible = false;
    }

    private function playButtonClick (event : MouseEvent) : void
    {
        doPlay ();
    }

    private function doPlay () : void
    {
	playing = true;

	play_button.visible = false;
	grey_shadow.visible = false;

	if (play_timer) {
	    clearInterval (play_timer);
	    play_timer = 0;
	}
	play_timer = setInterval (playTimerTick, play_duration);

	if (conn) {
            if (stream)
                stream.resume ();
	} else {
	    doSetChannel (channel_uri, stream_name, true /* reconnect */);
	}

	showButtons ();
    }

    private function showButtons () : void
    {
	buttons_visible = true;

	fullscreen_button.visible = true;
	horizontal_button.visible = !videoShouldBeHorizontal();
    }

    private function hideButtons () : void
    {
	buttons_visible = false;

	fullscreen_button.visible = false;
	horizontal_button.visible = false;
    }

    private function hideButtonsTick () : void
    {
	hideButtons ();
    }

    private function onMouseMove (event : MouseEvent) : void
    {
	if (hide_buttons_timer) {
	    clearInterval (hide_buttons_timer);
	    hide_buttons_timer = 0;
	}

	if (playing && video.visible) {
	    hide_buttons_timer = setInterval (hideButtonsTick, 5000);
	    showButtons ();
        }
    }

    public function BasicPlayer ()
    {
	playing = false;
	reconnecting = false;

	if (loaderInfo.parameters ["buffer"])
	    buffer_time = loaderInfo.parameters ["buffer"];
        else
	    buffer_time = 1.0;

        if (loaderInfo.parameters ["autoplay"])
            start_paused = false;
        else
            start_paused = true;

	buffering_complete = false;
	frame_no = 0;

	buttons_visible = false;
	horizontal_mode = false;

	reconnect_interval = 0;
	reconnect_timer = 0;

	play_duration = loaderInfo.parameters ["play_duration"] * 1000;
	if (play_duration == 0)
	    play_duration = 60000;

	play_timer = 0;

	hide_buttons_timer = setInterval (hideButtonsTick, 5000);

	stage.scaleMode = StageScaleMode.NO_SCALE;
	stage.align = StageAlign.TOP_LEFT;

        stage_width = stage.stageWidth;
        stage_height = stage.stageHeight;

	video = new Video();
	video.width  = 320;
	video.height = 240;
	video.smoothing = true;
//	video.deblocking = 5;
        video.visible = false;

	addChild (video);

	var fullscreen_button_bitmap : Bitmap = new FullscreenButton();
	fullscreen_button = new Sprite();
	fullscreen_button.addChild (fullscreen_button_bitmap);
	fullscreen_button.visible = false;
	addChild (fullscreen_button);
	fullscreen_button.addEventListener (MouseEvent.CLICK, toggleFullscreen);

	var horizontal_button_bitmap : Bitmap = new HorizontalButton();
	horizontal_button = new Sprite();
	horizontal_button.addChild (horizontal_button_bitmap);
	horizontal_button.visible = false;
	addChild (horizontal_button);
	horizontal_button.addEventListener (MouseEvent.CLICK, toggleHorizontal);

	if (loaderInfo.parameters ["shadow"])
	    shadow_opacity = loaderInfo.parameters ["shadow"];
	else
	    shadow_opacity = 0.4;

	grey_shadow = new Sprite();
	grey_shadow.graphics.beginFill (0x000000, shadow_opacity);
	grey_shadow.graphics.drawRect (0.0, 0.0, width, height);
	grey_shadow.graphics.endFill ();
	grey_shadow.x = 0;
	grey_shadow.y = 0;
	addChild (grey_shadow);

	play_button_bitmap = new PlayButton();
	play_button_hover_bitmap = new PlayButtonHover();
	play_button_hover_bitmap.visible = false;
	play_button = new Sprite();
	play_button.addChild (play_button_bitmap);
	play_button.addChild (play_button_hover_bitmap);
	addChild (play_button);
	play_button.addEventListener (MouseEvent.MOUSE_OVER, playButtonMouseOver);
	play_button.addEventListener (MouseEvent.MOUSE_OUT,  playButtonMouseOut);
	play_button.addEventListener (MouseEvent.CLICK, playButtonClick);

	doResize ();
        stage.addEventListener ("resize",
            function (event : Event) : void {
		doResize ();
            }
        );

        stage.addEventListener ("mouseMove", onMouseMove);

	var server_uri : String = loaderInfo.parameters ["server"];
	var stream_name : String = loaderInfo.parameters ["stream"];

	setChannel (server_uri, stream_name);

        if (!start_paused)
            doPlay ();
    }
}

}

