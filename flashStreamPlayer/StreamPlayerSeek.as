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
	
	public class StreamPlayerSeek extends MovieClip {

		protected var oConnection:NetConnection;
		protected var oMetaData:Object = new Object();
		protected var oNetStream:NetStream;
		protected var oVideo:Video;
		protected var sStreamName:String = new String("");
		protected var iStart:int = 0;
		protected var iDuration:int = 60;
		protected var oURL:String = new String("");
		protected var oStartEdit:TextField;
		protected var oURLEdit:TextField;
		protected var oDurationEdit:TextField;
		protected var oDebugEdit:TextField;
		protected var oSeekEdit:TextField;
		

		/* the constructor */
		public function StreamPlayerSeek() {

			oURLEdit = new TextField();
			oURLEdit.x = 10;
			oURLEdit.y = 10;
			oURLEdit.width = 210;
			oURLEdit.appendText("enter URL");
			//oURLEdit.addEventListener(KeyboardEvent.KEY_DOWN, clickHandler);
			oURLEdit.selectable = true;
			oURLEdit.type = TextFieldType.INPUT;

			oStartEdit = new TextField();
			oStartEdit.x = 10;
			oStartEdit.y = 30;
			oStartEdit.width = 210;
			oStartEdit.appendText("0");
			oStartEdit.selectable = true;
			oStartEdit.type = TextFieldType.INPUT;

			oDurationEdit = new TextField();
			oDurationEdit.x = 10;
			oDurationEdit.y = 50;
			oDurationEdit.width = 210;
			oDurationEdit.appendText("60");
			oDurationEdit.selectable = true;
			oDurationEdit.type = TextFieldType.INPUT;

			oSeekEdit = new TextField();
			oSeekEdit.x = 10;
			oSeekEdit.y = 100;
			oSeekEdit.width = 210;
			oSeekEdit.appendText("enter seek value [0:100]");
			oSeekEdit.addEventListener(KeyboardEvent.KEY_DOWN, SeekHandler);
			oSeekEdit.selectable = true;
			oSeekEdit.type = TextFieldType.INPUT;

			oDebugEdit = new TextField();
			oDebugEdit.x = 10;
			oDebugEdit.y = 200;
			oDebugEdit.width = 210;
			oDebugEdit.appendText("debug info");

			oVideo = new Video(640, 480);

			oConnection = new NetConnection();
			oConnection.addEventListener(NetStatusEvent.NET_STATUS, eNetStatus, false, 0, true);
			
			NetConnection.prototype.onBWDone = function(oObject1:Object):void {
				trace("onBWDone: " + oObject1.toString()); // some media servers are dumb, so we need to catch a strange event..
			}

			addChild(oURLEdit);
			addChild(oStartEdit);
			addChild(oDurationEdit);
			addChild(oSeekEdit);
			addChild(oDebugEdit);
		}

		/* triggered when meta data is received. */
		protected function eMetaDataReceived(oObject:Object):void {
			trace("MetaData: " + oObject.toString()); // debug trace..
		}
		
		private function SeekHandler(event:KeyboardEvent) :void {
			if(event.charCode == 13){

				var iSeekPercent:int = int(oSeekEdit.getLineText(0));
				if(iSeekPercent > 100 || iSeekPercent < 0)
					return;

				iStart = int(oStartEdit.getLineText(0));
				iDuration = int(oDurationEdit.getLineText(0));

				var seekStart:int = iStart + (iDuration * iSeekPercent) / 100;

				var str:String = oURLEdit.getLineText(0);
				var my_array:Array = str.split("/");
				sStreamName = my_array[my_array.length-1];
				sStreamName += "?start=";
				sStreamName += seekStart.toString();

				oDebugEdit.text = "Time" + seekStart.toString();
				
				oURL = oURLEdit.getLineText(0);
				oURLEdit.text=this.oURL;
				oConnection.connect(this.oURL);	
			}
		}
		/*
		private function clickHandler(event:KeyboardEvent) :void {
			if(event.charCode == 13){

				var str:String = oURLEdit.getLineText(0);
				var my_array:Array = str.split("/");
				sStreamName = my_array[my_array.length-1];
				sStreamName += "?start=";
				sStreamName += oStartEdit.getLineText(0);

				iStart = int(oStartEdit.getLineText(0));
				iDuration = int(oDurationEdit.getLineText(0));

				//oDebugEdit.appendText("|||" + sStreamName + "|||");
				
				oURL = oURLEdit.getLineText(0);
				oURLEdit.text=this.oURL;
				oConnection.connect(this.oURL);	
			}
		}
		*/

		private function eNetStatus(oEvent1:NetStatusEvent):void {

			trace("   NetStatusEvent: " + oEvent1.info.code + "   "); // debug trace..

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

				trace("   Connected to the RTMP server.   "); // debug trace..
				break;

			case "NetConnection.Connect.Closed":

				trace("   Disconnected from the RTMP server.   "); // debug trace..
				break;

			case "NetStream.Play.StreamNotFound":

				trace("   This stream is currently unavailable.   "); // debug trace..
				break;

			}
		}
	}
}
