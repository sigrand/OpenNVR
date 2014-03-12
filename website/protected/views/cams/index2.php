<script>
function togglePlaylist() {
    menuSources = document.getElementById("menuSources");
    menuArchive = document.getElementById("menuArchive");
    flash_div = document.getElementById("flash_div");
    progressbar = document.getElementById("progressbar");
    progressButton = document.getElementById("progressButton");

    if (menuSources.style.display == "none") {
        menuSources.style.display = "block";
        menuArchive.style.display = "block";
        flash_div.style.paddingRight = "270px";
        progressbar.style.width = "calc(100% - 270px)";
        progressButton.style.width = "calc(100% - 270px)";
    } else {
        menuSources.style.display = "none";
        menuArchive.style.display = "none";
        flash_div.style.paddingRight = "0px";
        progressbar.style.width = "100%";
        progressButton.style.width = "100%";
    }
}
function pad2(a) {
    if (a < 10)
        return '0' + a;

    return '' + a;
}
function unixtimeToDate(unixtime) {
    var date = new Date(unixtime * 1000);
    var date_str = date.getFullYear() + '.'
        + (date.getMonth() + 1) + '.'
        + date.getDate() + ' '
        + pad2(date.getHours()) + ':'
        + pad2(date.getMinutes()) + ':'
        + pad2(date.getSeconds());
    return date_str;
}

var cursor_position_sel;
var play_position_sel;
var reference_pos = 0;
var play_start_pos = 0;
var last_time_delta = 0;

function getPosInputUnixtime() {
    var date = new Date($("#pos-input-year").val(),
        $("#pos-input-month").val() - 1,
        $("#pos-input-day").val(),
        $("#pos-input-hour").val(),
        $("#pos-input-minute").val(),
        $("#pos-input-second").val(),
        0);
    var time = Math.floor(date.getTime() / 1000);
    return time;
}

function setCursorPosition(unixtime) {
    cursor_position_sel.html(unixtimeToDate(unixtime));
}

function setPlayStartPosition(unixtime, set_input_pos) {
    if (set_input_pos) {
        var date = new Date(unixtime * 1000);
        $("#pos-input-year").val(date.getFullYear());
        $("#pos-input-month").val(date.getMonth() + 1);
        $("#pos-input-day").val(date.getDate());
        $("#pos-input-hour").val(pad2(date.getHours()));
        $("#pos-input-minute").val(pad2(date.getMinutes()));
        $("#pos-input-second").val(pad2(date.getSeconds()));
    }
    play_start_pos = cur_position_playing_archive;
    last_time_delta = 0;
    setPlayPosition(0);
}

function setPlayPosition(time_delta) {
    if (isNaN(time_delta))
        time_delta = last_time_delta;
    else
        last_time_delta = time_delta;
    play_position_sel.html(unixtimeToDate(play_start_pos + time_delta));

    var z = end_playing_archive - start_playing_archive;
    var y = $(".progressbar").width();
    var x = time_delta;

    if (cur_position_playing_archive == 0)
        var g = 0;
    else
        var g = cur_position_playing_archive - start_playing_archive
    $("#mark-play")[0].style.left = ((g + x) * (y / z)) + 'px';
//console.log('play_start_pos' + play_start_pos + ' <-reference_pos' + reference_pos + '<-' + start_playing_archive + ' ' + end_playing_archive);
}


var recording_on = false;
function showRecording() {
    if (recording_on) {
        $("#rec-button").html("REC OFF");
        $("#rec-button").attr("class", "go-button rec-button-on");
    } else {
        $("#rec-button").html("REC ON");
        $("#rec-button").attr("class", "go-button rec-button-off");
    }
}

function processStateReply(reply) {
    state = $.parseJSON(reply);
    if (state.seq != state_seq)
        return;

    recording_on = state.recording;
    showRecording();
}

var state_seq = 0;
function requestState() {
    state_seq = state_seq + 1;
//$.get ("/mod_nvr/channel_state?stream=Sigrand_1C-131&seq=" + state_seq,
//  {}, processStateReply);
}

function doDownload(download) {
    /*
     var duration = $("#download-minutes").val() * 60;
     if (duration == 0)
     duration = 60;
     var link =  "http://sigrand.ru/mod_nvr/file.mp4?stream=Sigrand_1C-131&start="
     + getPosInputUnixtime()
     + "&duration=" + duration + (download ? "&download" : "");
     if (download)
     window.location.href = link;
     else
     window.open (link);*/
}

function flashInitialized() {
    flash_initialized = true;
    if (first_uri)
        document["MyPlayer"].setFirstUri(first_uri, first_stream_name);
}
function updatePlaylist() {
    $.ajax({
        cache: false,
        type: "GET",
        url: "<?php echo $this->createUrl('cams/playlist'); ?>",
        contentType: "application/json",
        dataType: "json",
        data: null,
        crossDomain: true,
        error: function (jqXHR, ajaxOptions, thrownError) {
            console.log('Error: ' + jqXHR + ' | ' + jqXHR);
            console.log('Error: ' + ajaxOptions + ' | ' + thrownError + ' | ' + jqXHR);
        },
        success: function (data) {
            var playlist = data["sources"];
            var class_one = "menuentry_one";
            var class_two = "menuentry_two";

            while (menuSourcesList.hasChildNodes()) {
                menuSourcesList.removeChild(menuSourcesList.lastChild);
            }
            console.log("playlist.length is " + playlist.length);
            for (var i = 0; i < playlist.length; ++i) {
                var item = playlist[i];

                var entrySource = document.createElement("div");
                entrySource.className = (i % 2 === 0) ? class_one : class_two;
                entrySource.style.padding = "10px";
                entrySource.style.textAlign = "left";
                entrySource.id = item["name"];
                entrySource.style.verticalAlign = "bottom";
                entrySource.onclick = (function (uri, stream_name, index, entrySource) {
                    return function () {
                        $.ajax({
                            cache: false,
                            type: "GET",
                            url: "<?php echo $this->createUrl('cams/existence'); ?>/" + stream_name,
                            contentType: "application/json",
                            dataType: "json",
                            data: null,
                            crossDomain: true,
                            error: function (jqXHR) {
                                console.log('Error: ' + jqXHR.status + ' ' + jqXHR.statusText);
                                var children = document.getElementById('menuSourcesList').childNodes;
                                for (var i = 0; i < children.length; ++i) {
                                    children[i].style.backgroundColor = (i % 2 === 0) ? "#333333" : "#222222";
                                }

                                cur_playing = index;
                                console.log("YYYY cur_playing is " + cur_playing);
                                entrySource.style.backgroundColor = "#666666";
                            },
                            success: function (data) {
                                if (data && data["timings"]) {
                                    while (menuArchiveList.hasChildNodes()) {
                                        menuArchiveList.removeChild(menuArchiveList.lastChild);
                                    }

                                    var children = document.getElementById('menuSourcesList').childNodes;
                                    for (var i = 0; i < children.length; ++i) {
                                        children[i].style.backgroundColor = (i % 2 === 0) ? "#333333" : "#222222";
                                    }

                                    cur_playing = index;
                                    console.log("cur_playing is " + cur_playing);
                                    entrySource.style.backgroundColor = "#666666";

                                    $.each(data["timings"], function (index, value) {
                                        if (value && value["start"] && value["end"]) {
                                            var entryArchive = document.createElement("div");
                                            entryArchive.className = (index % 2 === 0) ? class_one : class_two;
                                            entryArchive.style.padding = "10px";
                                            entryArchive.style.textAlign = "left";
                                            entryArchive.style.verticalAlign = "bottom";
                                            entryArchive.style.fontSize = "small";
                                            entryArchive.onclick = (function (uri, stream_name, time_start, time_end, index) {
                                                return function () {
                                                    var uriParsered = parseUri(uri);
                                                    document["MyPlayer"].setSourceSeeked(uriParsered.protocol + "://" + uriParsered.host + ":" + uriParsered.port + "/nvr/" + stream_name, stream_name, time_start, time_end);

                                                    console.log('(setSourceSeeked).click  ' + uriParsered.protocol + uriParsered.host + uriParsered.port + stream_name + time_start + ' ' + time_end);

                                                    start_playing_archive = time_start;
                                                    cur_position_playing_archive = time_start;
                                                    end_playing_archive = time_end;

                                                    cur_playing_archive = index;
                                                    console.info(start_playing_archive + "<- start and stop -> " + end_playing_archive);
                                                    $(document).ready(function () {
                                                        cursor_position_sel = $("#cursor-position");
                                                        play_position_sel = $("#play-position");
                                                        var mark_cursor_sel = $("#mark-cursor")[0];
                                                        var mark_cursor_x = 0;

                                                        $(window).resize(function () {
                                                            doResize();
                                                        });

                                                        doResize();

                                                        /* TODO POST */
                                                        $.get("<?php echo 'http://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_web_port']; ?>/mod_nvr/unixtime",
                                                            {},
                                                            function (dataTime) {
                                                                var unixtime = eval('(' + dataTime + ')');
                                                                var local_unixtime = Math.floor((new Date).getTime() / 1000);

                                                                /*var date = new Date (unixtime * 1000);*/

                                                                /* var unixtime = Math.floor (date.getTime() / 1000); */

                                                                reference_pos = data["timings"][cur_playing_archive]["start"];
                                                                console.log(cur_playing_archive + " do function cur_playing_archive");
                                                                setPlayStartPosition(unixtime, true);

                                                                $("#progressbar").click(function (e) {

                                                                    var z = (data["timings"][cur_playing_archive]["end"] - data["timings"][cur_playing_archive]["start"]);
                                                                    var y = $(".progressbar").width();
                                                                    var x = e.pageX;
                                                                    var time = Math.floor(reference_pos + (x * (z / y)));

                                                                    setPlayStartPosition(time, true);

                                                                    time_end = data["timings"][cur_playing_archive]["end"];

                                                                    document["MyPlayer"].setSourceSeeked(uriParsered.protocol + "://" + uriParsered.host + ":" + uriParsered.port + "/nvr/" + stream_name, stream_name, time, time_end);

                                                                    cur_position_playing_archive = time;
                                                                });

                                                                $("#progressbar").mousemove(function (e) {

                                                                    var z = (data["timings"][cur_playing_archive]["end"] - data["timings"][cur_playing_archive]["start"]);
                                                                    var y = $(".progressbar").width();
                                                                    var x = e.pageX;
//console.log(e.pageX + " e.pageX " + z +  "  " + y + ' ' + x);
                                                                    var time = Math.floor(reference_pos + (x * (z / y)));
                                                                    mark_cursor_x = x;
                                                                    mark_cursor_sel.style.left = mark_cursor_x + 'px';
                                                                    setCursorPosition(time);
                                                                });

                                                                $("#go-button").click(function (e) {
                                                                    var time = getPosInputUnixtime();
                                                                    mark_cursor_sel.style.left = (mark_cursor_x + reference_pos - time) + 'px';
                                                                    reference_pos = time;
                                                                    setPlayStartPosition(time, true);
                                                                    document["MyPlayer"].setSourceSeeked(uriParsered.protocol + "://" + uriParsered.host + ":" + uriParsered.port + "/nvr/" + stream_name, stream_name, time, time_end);
                                                                });

                                                                $("#download-button").click(function (e) {
                                                                    doDownload(true);
                                                                });

                                                                $("#watch-button").click(function (e) {
                                                                    doDownload(false);
                                                                });

                                                                $("#live-button").click(function (e) {
                                                                    var time = getPosInputUnixtime();
                                                                    document ["MyPlayer"].setSource(uri, stream_name + "?start=" + time);
                                                                });

                                                                setInterval(
                                                                    function () {
                                                                        setPlayPosition(document["MyPlayer"].getPlayheadTime());
                                                                    },
                                                                    1000);

                                                                requestState();
                                                                setInterval(
                                                                    function () {
                                                                        requestState();
                                                                    },
                                                                    1000);

                                                                $("#rec-button").click(function (e) {
                                                                    var was_on = recording_on;

                                                                    recording_on = !recording_on;
                                                                    showRecording();

                                                                    state_seq = state_seq + 1;
                                                                    $.get("/mod_nvr_admin/rec_" + (was_on ? "off" : "on") + "?stream=Sigrand_1C-131&seq=" + state_seq,
                                                                        {}, processStateReply);
                                                                });
                                                            }
                                                        );
                                                    });
                                                }
                                            })(uri, stream_name, value["start"], value["end"], index);
                                            entryArchive.innerHTML = '<table style="width: 100%"><tr><td style="width: 40%; height: 100%; text-align: center">' +
                                                unixtimeToDate(value["start"]) + '<br />' + '</td><td style="width: 20%; height: 100%; text-align: center">-</td><td style="width: 40%; height: 100%; text-align: center">' +
                                                unixtimeToDate(value["end"]) + '<br />' + '</td></tr></table>';

                                            menuArchiveList.appendChild(entryArchive);
                                        }
                                    });
                                }
//
                            }
                        });
                        console.log("uri - ", uri);
                        console.log("stream_name - ", stream_name);
                        document["MyPlayer"].setSource(uri, stream_name);
                    };
                })(item["uri"], item["name"], i, entrySource);
                entrySource.innerHTML = item["title"];
                menuSourcesList.appendChild(entrySource);
            }
            first_uri = playlist[0]["uri"];
            first_stream_name = playlist[0]["name"];
            if (first_uri && flash_initialized) {
                document["MyPlayer"].setFirstUri(first_uri, first_stream_name);
            }
        }
    });
}

function doResize() {
//$("#mark-middle")[0].style.left = /*Math.floor ($(window).width() / 2)*/  0  + 'px';
    $("#mark-play")[0].style.left = Math.floor(last_time_delta) + 'px';
}

</script>
<?php
/* @var $this CamsController */

if (!Yii::app()->user->isGuest) {
    ?>
    <div class="col-sm-12">
        <div class="panel panel-default">
            <div class="panel-heading">
                <h3 class="panel-title"><?php echo Yii::t('cams', 'My cams'); ?></h3>
            </div>
            <div class="panel-body">
                <ul class="list-group">
                    <li class="list-group-item">
                        <div class="row">
                            <?php if (!empty($myCams)) {
                            foreach ($myCams as $key => $cam) {
                            ?>
                            <div class="col-sm-6">
                                <div id="flash_div"
                                     style="height: 100%; padding-right: <?php echo $this->showArchive ? '15' : '0'; ?>%;">
                                    <object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" width="100%" height="100%" id="MyPlayer" align="middle">
                                        <param name="movie"
                                               value="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer.swf"/>
                                        <param name="allowScriptAccess" value="always"/>
                                        <param name="quality" value="high"/>
                                        <param name="scale" value="noscale"/>
                                        <param name="salign" value="lt"/>
                                        <param name="bgcolor" value="#000000"/>
                                        <param name="allowFullScreen" value="true"/>
                                        <param name="FlashVars" value="autoplay=0&playlist=1&buffer=2.0"/>
                                        <embed FlashVars="autoplay=0&playlist=1&buffer=2.0"
                                               src="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer.swf"
                                               bgcolor="#000000"
                                               width="100%"
                                               height="100%"
                                               name="MyPlayer"
                                               quality="high"
                                               align="middle"
                                               scale="showall"
                                               allowFullScreen="true"
                                               allowScriptAccess="always"
                                               type="application/x-shockwave-flash"
                                               pluginspage="http://www.macromedia.com/go/getflashplayer"
                                            />
                                    </object>
                                </div>
                                <br/>
                                <?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
                                <?php echo $cam->desc; ?><br/>
                            </div>
                            <?php
                            if ($key % 2) {
                            ?>
                        </div>
                    </li>
                    <li class="list-group-item">
                        <div class="row">

                            <?php
                            }
                            }
                            ?>
                            <script type="text/javascript">
                                var cam = $(".cam_0");
                                var h = cam.innerWidth() / 1.3;
                                $('object[class*="cam_"]').height(h).show();
                                cam.height(h).show();

                               /* $('object[class*="cam_"]').each(function(){
                                    var t = this;
                                    var iv = setInterval(function () {

                                        alert(typeof $(t) == 'object');
                                        if (typeof $(t) == 'object') {
                                            clearInterval(iv);
                                            alert(1);
                                            $(t).setSourceSeeked('', $(t).attr('camid'));
                                            //alert();
                                        }
                                    }, 100);
                                });
                                    /*
                                var iv = setInterval(function () {
                                    //alert('<?php echo $cam->id; ?>');
                                    if ($('#<?php echo $cam->id; ?>').html()) {
                                        clearInterval(iv);
                                        // alert($('#<?php echo $cam->id; ?>').html());
                                        $('#<?php echo $cam->id; ?>').click()
                                    }
                                }, 100);
                                //*/
                            </script>
                            <?php
                            } else {
                                echo Yii::t('cams', 'No cams to show.');
                            } ?>
                </ul>
            </div>
        </div>
    </div>
    <div class="col-sm-12">
        <div class="panel panel-default">
            <div class="panel-heading">
                <h3 class="panel-title"><?php echo Yii::t('cams', 'Cams shared for me'); ?></h3>
            </div>
            <div class="panel-body">
                <ul class="list-group">
                    <li class="list-group-item">
                        <div class="row">
                            <?php if (!empty($mySharedCams)) {
                            foreach ($mySharedCams as $key => $cam) {
                            if (isset($cam->cam_id)) {
                                $cam = $cam->cam;
                            }
                            ?>
                            <div class="col-sm-6">
                                <object type="application/x-shockwave-flash"
                                        data="<?php echo Yii::app()->request->baseUrl . '/player/uppod.swf'; ?>"
                                        class="img-thumbnail" id="scam_<?php echo $key; ?>"
                                        style="width:100%;display:none;">
                                    <param name="bgcolor" value="#ffffff"/>
                                    <param name="allowFullScreen" value="true"/>
                                    <param name="allowScriptAccess" value="always"/>
                                    <param name="wmode" value="window"/>
                                    <param name="movie"
                                           value="<?php echo Yii::app()->request->baseUrl . '/player/uppod.swf'; ?>"/>
                                    <param name="flashvars"
                                           value="aspect=1.33&st=<?php echo Yii::app()->request->baseUrl . '/player/style.txt'; ?>&file=<?php echo 'rtmp://' . Yii::app()->params['moment_server_ip'] . ':' . Yii::app()->params['moment_live_port'] . '/live/' . $cam->id; ?>">
                                </object>
                                <br/>
                                <?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
                                <?php echo $cam->desc; ?><br/>
                            </div>
                            <?php
                            if ($key % 2) {
                            ?>
                        </div>
                    </li>
                    <li class="list-group-item">
                        <div class="row">
                            <?php
                            }
                            }
                            ?>
                            <script type="text/javascript">
                                var cam = $("#scam_0");
                                var h = cam.innerWidth() / 1.3;
                                $('object[id*="scam_"]').height(h).show();
                                cam.height(h).show();
                            </script>
                        </div>
                    </li>
                    <?php
                    } else {
                        echo Yii::t('cams', 'No cams to show.');
                    } ?>
                </ul>
            </div>
        </div>
    </div>
<?php
}
?>
<div class="col-sm-12">
    <div class="panel panel-default">
        <div class="panel-heading">
            <h3 class="panel-title"><?php echo Yii::t('cams', 'Public cams'); ?></h3>
        </div>
        <div class="panel-body">
            <ul class="list-group">
                <li class="list-group-item">
                    <div class="row">
                        <?php if (!empty($myPublicCams)) {
                        foreach ($myPublicCams as $key => $cam) {
                        if (isset($cam->cam_id)) {
                            $cam = $cam->cam;
                        }
                        ?>
                        <div class="col-sm-6">
                            <object type="application/x-shockwave-flash"
                                    data="<?php echo Yii::app()->request->baseUrl . '/player/uppod.swf'; ?>"
                                    class="img-thumbnail" id="pcam_<?php echo $key; ?>"
                                    style="width:100%;display:none;">
                                <param name="bgcolor" value="#ffffff"/>
                                <param name="allowFullScreen" value="true"/>
                                <param name="allowScriptAccess" value="always"/>
                                <param name="wmode" value="window"/>
                                <param name="movie"
                                       value="<?php echo Yii::app()->request->baseUrl . '/player/uppod.swf'; ?>"/>
                                <param name="flashvars"
                                       value="aspect=1.33&st=<?php echo Yii::app()->request->baseUrl . '/player/style.txt'; ?>&file=<?php echo 'rtmp://' . Yii::app()->params['moment_server_ip'] . ':' . Yii::app()->params['moment_live_port'] . '/live/' . $cam->id; ?>">
                            </object>
                            <br/>
                            <?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
                            <?php echo $cam->desc; ?><br/>
                        </div>
                        <?php
                        if ($key % 2) {
                        ?>
                    </div>
                </li>
                <li class="list-group-item">
                    <div class="row">
                        <?php
                        }
                        }
                        ?>
                        <script type="text/javascript">
                            var cam = $("#pcam_0");
                            var h = cam.innerWidth() / 1.3;
                            $('object[id*="pcam_"]').height(h).show();
                            cam.height(h).show();
                        </script>
                    </div>
                </li>
                <?php
                } else {
                    echo Yii::t('cams', 'No cams to show.');
                } ?>
            </ul>
        </div>
    </div>
</div>
<div id="menuSourcesList" style="display: none;"></div>
<div id="menuArchiveList" style="display: none;"></div>
<script type="text/javascript">
    var flash_initialized = false;
    var first_uri;
    var first_stream_name;
    var cur_playing = -1;
    var cur_playing_archive = -1;
    var start_playing_archive = 0;
    var end_playing_archive = 0;
    var cur_position_playing_archive = 0;
    updatePlaylist();
    var class_one = "menuentry_one";
    var class_two = "menuentry_two";
    var menuSourcesList = document.getElementById("menuSourcesList");
    var menuArchiveList = document.getElementById("menuArchiveList");
    //alert($('#<?php echo $cam->id; ?>').text());
    alert(typeof document["MyPlayer"]);
    document["MyPlayer"].setSourceSeeked('', '30', '', '');
   // iv();
    //$('#<?php echo $cam->id; ?>').click();
    //*/
</script>