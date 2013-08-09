package {

import flash.display.Sprite;
import flash.display.StageScaleMode;
import flash.display.StageAlign;
import flash.media.Camera;
import flash.media.Microphone;
import flash.media.Video;
import flash.media.SoundCodec;
import flash.media.H264VideoStreamSettings;
import flash.media.H264Level;
import flash.media.H264Profile;
import flash.net.NetConnection;
import flash.net.NetStream;
import flash.net.ObjectEncoding;
import flash.events.NetStatusEvent;
import flash.utils.setInterval;
import flash.utils.clearInterval;

[SWF(backgroundColor=0)]
public class Publisher extends Sprite
{
    private var server_uri  : String;
    private var stream_name : String;
    private var enable_cam  : Boolean;
    private var enable_mic  : Boolean;
    private var enable_h264 : Boolean;
    private var enable_aec  : Boolean;

    private var conn : NetConnection;
    private var stream : NetStream;
    private var video : Video;

    private var camera : Camera;
    private var microphone : Microphone;

    private var reconnect_interval : uint;
    private var reconnect_timer : uint;

    public function Publisher ()
    {
        stage.scaleMode = StageScaleMode.NO_SCALE;
        stage.align = StageAlign.TOP_LEFT;

        reconnect_interval = 3000;

        server_uri  = loaderInfo.parameters ["uri"];
        stream_name = loaderInfo.parameters ["stream"];
        enable_cam  = loaderInfo.parameters ["enable_cam"]  == "true";
        enable_mic  = loaderInfo.parameters ["enable_mic"]  == "true";
        enable_h264 = loaderInfo.parameters ["enable_h264"] == "true";
        enable_aec  = loaderInfo.parameters ["enable_aec"]  == "true";

        video = new Video();
        video.width  = 640;
        video.height = 480;
        video.x = 0;
        video.y = 0;
        addChild (video);

        if (enable_cam) {
            camera = Camera.getCamera();
            if (camera) {
//                camera.setMode (320, 240, 15);
                camera.setMode (160, 120, 15);
//                camera.setQuality (200000, 80);
                camera.setQuality (256000 / 8.0, 80);
                video.attachCamera (camera);
            }
        }

        if (enable_mic) {
            if (enable_aec)
                microphone = Microphone.getEnhancedMicrophone();
            else
                microphone = Microphone.getMicrophone();

            if (microphone) {
                microphone.codec = SoundCodec.SPEEX;
                microphone.framesPerPacket = 1;
                microphone.enableVAD = true;
                if (enable_aec) {
                    microphone.setSilenceLevel (0, 2000);
                }
                microphone.setUseEchoSuppression (true);
                microphone.setLoopBack (false);
                microphone.gain = 50;
            }
        }

        doConnect ();
    }

    private function doDisconnect () : void
    {
//        if (stream)
//            stream.removeEventListener (NetStatusEvent.NET_STATUS, onStreamNetStatus);

        if (conn) {
            conn.removeEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);
            conn.close ();
            conn = null;
        }
    }

    private function doConnect () : void
    {
        if (reconnect_timer) {
            clearInterval (reconnect_timer);
            reconnect_timer = 0;
        }

        doDisconnect ();

        conn = new NetConnection();
        conn.objectEncoding = ObjectEncoding.AMF0;

        conn.addEventListener (NetStatusEvent.NET_STATUS, onConnNetStatus);
        conn.connect (server_uri);

        reconnect_timer = setInterval (reconnectTick, reconnect_interval);
    }

    private function onConnNetStatus (event : NetStatusEvent) : void
    {
        if (event.info.code == "NetConnection.Connect.Success") {
            if (reconnect_timer) {
                clearInterval (reconnect_timer);
                reconnect_timer = 0;
            }

            stream = new NetStream (conn);
            stream.bufferTime = 0.0;

            if (camera)
                video.attachCamera (camera);

            if (enable_h264) {
                var avc_opts : H264VideoStreamSettings = new H264VideoStreamSettings ();
                avc_opts.setProfileLevel (H264Profile.BASELINE, H264Level.LEVEL_3_1);
// This has no effect                avc_opts.setQuality (1000000, 100);
//                avc_opts.setQuality (100000, 50);
                stream.videoStreamSettings = avc_opts;
            }

            stream.publish (stream_name);

            if (camera)
                stream.attachCamera (camera);

            if (microphone)
                stream.attachAudio (microphone);
        } else
        if (event.info.code == "NetConnection.Connect.Closed") {
            if (!reconnect_timer) {
                if (reconnect_interval == 0) {
                    doReconnect ();
                    return;
                }

                reconnect_timer = setInterval (reconnectTick, reconnect_interval);
            }
        }
    }

    private function reconnectTick () : void
    {
        doReconnect ();
    }

    private function doReconnect () : void
    {
        doConnect ();
    }
}

}

