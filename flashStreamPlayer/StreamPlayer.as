package  {

	import flash.events.NetStatusEvent;
	import flash.media.Video;
	import flash.net.NetConnection;
	import flash.net.NetStream;
	import flash.display.MovieClip;
	import flash.media.Video;
	import flash.net.NetConnection;
	import flash.net.NetStream;
	import flash.text.TextField;
	import flash.text.TextFieldType;

	import flash.events.KeyboardEvent;
	
	public class StreamPlayer extends MovieClip {

		protected var oConnection:NetConnection;
		protected var oMetaData:Object = new Object();
		protected var oNetStream:NetStream;
		protected var oVideo:Video;
		protected var sStreamName:String = new String("sony");
		protected var oURL:String = new String("");
		protected var oURLEdit:TextField;
		//protected var oStreamButton:Button;

		/* the constructor */
		public function StreamPlayer() {
			oURLEdit = new TextField();
			oURLEdit.x = 10;
			oURLEdit.y = 10;
			oURLEdit.width = 210;
			oURLEdit.appendText("enter URL");
			oURLEdit.addEventListener(KeyboardEvent.KEY_DOWN, clickHandler);
			oURLEdit.selectable = true;
			oURLEdit.type = TextFieldType.INPUT;

			oVideo = new Video(640, 480);

			oConnection = new NetConnection();
			oConnection.addEventListener(NetStatusEvent.NET_STATUS, eNetStatus, false, 0, true);
			
			NetConnection.prototype.onBWDone = function(oObject1:Object):void {
				trace("onBWDone: " + oObject1.toString()); // some media servers are dumb, so we need to catch a strange event..
			}

			addChild(oURLEdit);
		}

		/* triggered when meta data is received. */
		protected function eMetaDataReceived(oObject:Object):void {
			trace("MetaData: " + oObject.toString()); // debug trace..
		}
		
		
		private function clickHandler(event:KeyboardEvent) :void {
			if(event.charCode == 13){
				oURL = oURLEdit.getLineText(0);
				oURLEdit.text=";";
				oConnection.connect(this.oURL);	
			}
		}

		private function eNetStatus(oEvent1:NetStatusEvent):void {

			trace("NetStatusEvent: " + oEvent1.info.code); // debug trace..

			switch (oEvent1.info.code) {
			case "NetConnection.Connect.Success":

				// create a stream for the connection..
				this.oNetStream = new NetStream(oConnection);
				this.oNetStream.addEventListener(NetStatusEvent.NET_STATUS, eNetStatus, false, 0, true);
				this.oNetStream.bufferTime = 5; // set this to whatever is comfortable..

				// listen for meta data..
				this.oMetaData.onMetaData = eMetaDataReceived;
				this.oNetStream.client = this.oMetaData;

				// attach the stream to the stage..
				this.oVideo.attachNetStream(oNetStream);
				this.oNetStream.play(sStreamName);
				this.addChildAt(this.oVideo, 0);

				trace("Connected to the RTMP server."); // debug trace..
				break;

			case "NetConnection.Connect.Closed":

				trace("Disconnected from the RTMP server."); // debug trace..
				break;

			case "NetStream.Play.StreamNotFound":

				trace("This stream is currently unavailable."); // debug trace..
				break;

			}
		}
	}
}
