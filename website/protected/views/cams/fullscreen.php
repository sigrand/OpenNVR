<!--
<object type="application/x-shockwave-flash" data="<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>" id="camplayer">
	<param name="bgcolor" value="#ffffff" />
	<param name="allowFullScreen" value="true" />
	<param name="allowScriptAccess" value="always" />
	<param name="wmode" value="window" />
	<param name="movie" value="<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>" />
	<param id="values" name="flashvars" value="&uid=camplayer&aspect=1.33&st=<?php echo Yii::app()->request->baseUrl.'/player/style.txt'; ?>&file=<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>">
</object>
<script type="text/javascript" src="<?php echo Yii::app()->request->baseUrl.'/js/uppod_api.js'; ?>"></script>
-->

<div id="cam"></div>
<script type="text/javascript">
var flash = $("#flash_div");
var flashvars = {
	file: "<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>",
	aspect: 1.33,
	st: "<?php echo Yii::app()->request->baseUrl.'/player/style.txt'; ?>"
}; 
var params = {
	bgcolor:"#ffffff",
	wmode:"window",
	allowFullScreen:"true",
	allowScriptAccess:"always"
}; 
swfobject.embedSWF("<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>", "cam", flash.width(), flash.height(), "10.0.0.0", false, flashvars, params); 
</script>
<script type="text/javascript">
function pad2 (a) {
	if (a < 10)
		return '0' + a;
	return '' + a;
}
function unixtimeToDate(unixtime) {
	var date = new Date (unixtime * 1000);
	var date_str = date.getFullYear() + '.'
	+ (date.getMonth() + 1) + '.'
	+ date.getDate() + ' '
	+ pad2 (date.getHours()) + ':'
	+ pad2 (date.getMinutes()) + ':'
	+ pad2 (date.getSeconds());
	return date_str;
}
function updatePlaylist(){
	$.ajax({
		cache: false,
		type: "GET",
		url: "<?php echo 'http://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_web_port']; ?>/mod_nvr/existence?stream=<?php echo $cam->id; ?>",
		contentType: "application/json",
		dataType: "json",
		data: null,
		crossDomain: true,
		error: function(jqXHR, ajaxOptions, thrownError) {
			console.log('Error: ' + jqXHR + ' | ' + jqXHR);
			console.log('Error: ' + ajaxOptions + ' | ' + thrownError + ' | ' + jqXHR);
		},
		success: function(data) {
			var playlist = data["timings"];
			for (var i = 0; i < playlist.length; ++i) {
				var item = playlist[i];
				var entryArchive = document.createElement("div");
				entryArchive.className = (i % 2 === 0) ? "menuentry_one" : "menuentry_two";
				entryArchive.style.padding = "10px";
				entryArchive.style.textAlign = "left";
				entryArchive.style.verticalAlign = "bottom";
				entryArchive.style.fontSize = "small";
				entryArchive.onclick = function () {
					var div = $("#cam");
					var newFlashvars = {
						file: "<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>?start="+item['start'],
						aspect: 1.33,
						st: "<?php echo Yii::app()->request->baseUrl.'/player/style.txt'; ?>"
					}; 
					swfobject.embedSWF("<?php echo Yii::app()->request->baseUrl.'/player/uppod.swf'; ?>", "cam", flash.width(), flash.height(), "10.0.0.0", false, newFlashvars, params); 
				};
				entryArchive.innerHTML = '<table style="width: 100%"><tr><td style="width: 40%; height: 100%; text-align: center">' +
				unixtimeToDate(item["start"]) + '<br />' + '</td><td style="width: 20%; height: 100%; text-align: center">-</td><td style="width: 40%; height: 100%; text-align: center">' +
				unixtimeToDate(item["end"]) + '<br />' + '</td></tr></table>';
				menuArchiveList.appendChild(entryArchive);
			}
		}
	}
	);
}
updatePlaylist();
</script>

<!--
<script type="text/javascript" src="http://releases.flowplayer.org/js/flowplayer-3.2.12.min.js"></script>
<a href="2"
    style="display:block;width:425px;height:300px;"
    id="cam">
</a>
<script language="JavaScript">
flowplayer("cam", "http://releases.flowplayer.org/swf/flowplayer-3.2.16.swf", {
 
        // configure both players to use rtmp plugin
        clip: {
        	url: '2',
        	live: true,
            provider: 'rtmp'
        },
 
        // here is our rtpm plugin configuration
        plugins: {
          rtmp: {
 
               // use latest RTMP plugin release
                url: "http://releases.flowplayer.org/swf/flowplayer.rtmp-3.2.12.swf",
 
                /
                    netConnectionUrl defines where the streams are found
                    this is what we have in our HDDN account
                /
                netConnectionUrl: 'rtmp://213.183.117.158:1935/live/'
          }
        }
 
});
</script>
<!--
<a class="rtmp" href="mp4:vod/demo.flowplayervod/bbb-800.mp4"
    style="background-image:url(http://flash.flowplayer.org/media/img/demos/bunny.jpg)">
    <img src="http://flash.flowplayer.org/media/img/player/btn/play_text_large.png" />
</a>
<script type="text/javascript">
<script>
head.js(
	"http://ajax.googleapis.com/ajax/libs/jquery/1.7/jquery.js",
	"http://cdn.jquerytools.org/1.2.6/all/jquery.tools.min.js",
	"http://releases.flowplayer.org/js/flowplayer-3.2.12.min.js", function(){
	});
head.ready(function() {
	$f("a.rtmp", "http://releases.flowplayer.org/swf/flowplayer-3.2.16.swf", {

        // configure both players to use rtmp plugin
        clip: {
        	provider: 'rtmp'
        },

        // here is our rtpm plugin configuration
        plugins: {
        	rtmp: {

               // use latest RTMP plugin release
               url: "http://releases.flowplayer.org/swf/flowplayer.rtmp-3.2.12.swf",

                /*
                    netConnectionUrl defines where the streams are found
                    this is what we have in our HDDN account
                    */
                    netConnectionUrl: 'rtmp://rtmp01.hddn.com/play'
                }
            }

        });
	</script>
	-->
	<!--
<script src="http://jwpsrv.com/library/zg+t_F0NEeOp_xIxOQfUww.js"></script>
<div id='cam'></div>
<script type='text/javascript'>
    jwplayer('cam').setup({
        file: 'rtmp://213.183.117.158:1935/live/2?start=1385698065',
        image: 'https://www.longtailvideo.com/content/images/jw-player/lWMJeVvV-876.jpg',
        aspectratio: '4:3',
        controls: 'false',
        fallback: 'false',
        autostart: 'true',
            rtmp: {
        subscribe: true
    },
    });
</script>
-->