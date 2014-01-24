<div id="cam"></div>
<script type="text/javascript">
var flash = $("#flash_div");
var flashvars = {
    name: 'cam',
    src: "<?php echo Yii::app()->request->baseUrl.'/player/MyPlayer.swf'; ?>",
    autoplay: 0,
    playlist: 1,
    buffer: 2.0,
    scale: "showall"
}; 
var params = {
    bgcolor:"#000",
    wmode:"window",
    allowFullScreen:"true",
    allowScriptAccess:"always"
}; 
swfobject.embedSWF(
    "<?php echo Yii::app()->request->baseUrl.'/player/MyPlayer.swf'; ?>",
    "cam",
    flash.width(),
    flash.height(),
    "10.0.0.0",
    false,
    flashvars,
    params
    ); 
</script>
<script type="text/javascript">
$(document).ready(function(){
    elemtype = $("#cam").get(0).tagName;
    if(elemtype != 'OBJECT') {
        setTimeout(function(){
            elemtype2 = $("#cam").get(0).tagName;
            if(elemtype2 == 'OBJECT') {
                document["cam"].setSourceSeeked('<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>', '<?php echo $cam->id; ?>', 0, 0);
            }
        }, 1000);
            }
        });
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
                        //alert(item['start']);
                        elemtype = $("#cam").get(0).tagName;
                        if(elemtype != 'OBJECT') {
                            setTimeout(function(){
                                elemtype2 = $("#cam").get(0).tagName;
                                if(elemtype2 == 'OBJECT') {
                                    //alert(1);
                                        document["cam"].setSourceSeeked('<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>', '<?php echo $cam->id; ?>', item['start'], item['end']);
                                    }
                                }, 2000);
                                    } else {
                                        //alert(1);
                                        document["cam"].setSourceSeeked('<?php echo 'rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/'.$cam->id; ?>', '<?php echo $cam->id; ?>', item['start'], item['end']);
                                    }
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