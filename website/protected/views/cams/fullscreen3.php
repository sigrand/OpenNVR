<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html style="height: 100%" xmlns="http://www.w3.org/1999/xhtml">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
    <title>Просмотр <?php echo $cam->name; ?></title>
    <style type="text/css">
    body {
        font-family: sans-serif;
        color: #000;
        background-color: #fff;
    }

    body {
        font-family: sans-serif;
        color: #000;
    }

    .menuentry_one {
        background-color: #eee;
    }

    .menuentry_two {
        background-color: #ddd;
    }

    div.menuentry_one:hover {
        background: #bbb;
        cursor: pointer;
    }

    div.menuentry_two:hover {
        background: #bbb;
        cursor: pointer;
    }

    .play-position {
        font-size: larger;
        color: /* #006600 */ #333333;
    }

    .cursor-position {
        font-size: larger;
        color: #2a52be;
    }

    .date-input {
        width: 2.1em;
        border: 1px solid #999999;
        padding: 4px 0px;
        text-align: right;
        font-size: medium;
        text-align: center;
    }

    .year-input {
        width: 3.1em;
    }

    .date-separator {
        padding: 1px;
        color: gray;
    }

    .go-button {
        color: white;
        /* background-color: #33aa33; */
        background-color: #229922;
        padding-left: 0.5em;
        padding-right: 0.5em;
    }

    .go-button:hover {
        cursor: pointer;
    }

    .rec-button-on {
        background-color: #cc3333;
    }

    .rec-button-off {
        background-color: #11bb11;
    }

    .download-button {
        background-color: #226699;
    }

    .watch-button {
        background-color: #4488bb;
    }

    .live-button {
        background-color: #884466;
    }

    .progressbar:hover {
        cursor: pointer;
    }
    </style>
    <link href="<?php echo Yii::app()->request->baseUrl; ?>/player/css/dark-hive/jquery-ui-1.10.4.custom.css" rel="stylesheet">
    <script src="<?php echo Yii::app()->request->baseUrl; ?>/player/js/jquery-1.10.2.js"></script>
    <script src="<?php echo Yii::app()->request->baseUrl; ?>/player/js/jquery-ui-1.10.4.custom.js"></script>
    <script type="text/javascript" src="<?php echo Yii::app()->request->baseUrl; ?>/player/url.js"></script>
    <script type="text/javascript">

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
        $('#progressbar').prop('title', unixtimeToDate(unixtime)).tooltip({
            position:{
                my: "left bottom-10",
                of: event,
                collision: "fit"
            },
            track:true,
            content:unixtimeToDate(unixtime)
        });
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
                                    var minDate = 0;
                                    var maxDate = 0;

                                    var children = document.getElementById('menuSourcesList').childNodes;
                                    for (var i = 0; i < children.length; ++i) {
                                        children[i].style.backgroundColor = (i % 2 === 0) ? "#333333" : "#222222";
                                    }

                                    cur_playing = index;
                                    console.log("cur_playing is " + cur_playing);
                                    entrySource.style.backgroundColor = "#666666";
                                    var c = data["timings"].length-1;
                                    $.each(data["timings"], function (index, value) {
                                        if(minDate == 0) { minDate = 1; $('.mindate').text(value["start"]*1000); }
                                        if(maxDate == 0 && index == c) { maxDate = 1; $('.maxdate').text(value["end"]*1000); }
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
                                                        $.get("<?php echo $this->createUrl('cams/unixtime'); ?>",
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
</head>
<body style="height: calc(100%-<?php echo $this->showStatusbar ? '40' : '0'; ?>px); padding: 0; margin: 0">
    <div style="min-height: 100%">
        <div
        style="position: absolute; width: 100%; top: 0px; bottom: <?php echo $this->showStatusbar ? '70' : '0'; ?>px; background-color: #fff">
        <div style="width: 100%; height: 100%; background-color: #fff">
            <div id="flash_div" style="height: 100%; padding-right: <?php echo $this->showArchive ? '15' : '0'; ?>%;">
                <object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" width="100%" height="100%" id="MyPlayer"
                align="middle">
                <param name="movie" value="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer.swf"/>
                <param name="allowScriptAccess" value="always"/>
                <param name="quality" value="high"/>
                <param name="scale" value="noscale"/>
                <param name="salign" value="lt"/>
                <param name="wmode" value="opaque"/>
                <param name="bgcolor" value="#000000"/>
                <param name="allowFullScreen" value="true"/>
                <param name="FlashVars" value="autoplay=0&playlist=1&buffer=0.3"/>
                <embed FlashVars="autoplay=0&playlist=1&buffer=0.3"
                src="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer.swf"
                bgcolor="#000000"
                width="100%"
                height="100%"
                name="MyPlayer"
                quality="high"
                wmode="opaque"
                align="middle"
                scale="showall"
                allowFullScreen="true"
                allowScriptAccess="always"
                type="application/x-shockwave-flash"
                pluginspage="http://www.macromedia.com/go/getflashplayer"
                />
            </object>
        </div>
    </div>
</div>


<div id="menuSources"
style="width: 15%; height: 50%; background-color: #111111; border-left: 1px solid #222222; overflow: auto; position: absolute; right: 0px; height: 40%; display:none;">

<div style="padding: 1.2em; border-bottom: 5px solid #444444; vertical-align: bottom; text-align: center">
    <span style="font-size: large; font-weight: bold; color: #777777">Sources</span>
</div>

<div id="menuSourcesList" style="width: 100%">
</div>
</div>
<?php if ($this->showArchive) { ?>
<div id="menuArchive"
style="width: 15%; height: 100%; background-color: #fff; border-left: 1px solid #222222; overflow: auto; position: absolute; bottom: 0px; right: 0px;">
<div style="padding: 1.2em; border-bottom: 5px solid #444444; vertical-align: bottom; text-align: center">
    <span style="font-size: large; font-weight: bold; color: #777777"><?php echo $cam->name; ?></span>
</div>
<div style="padding: 1.2em; border-bottom: 5px solid #444444; vertical-align: bottom; text-align: center">
    <span style="font-size: large; font-weight: bold; color: #777777">Архив</span>
</div>
<div id="menuArchiveList" style="width: 100%">
</div>
</div>
<?php
}
if ($this->showStatusbar) {
    ?>
    <div id="progressbar" class="progressbar"
    style="position: absolute; overflow: hidden; width: calc(100% - <?php echo $this->showArchive ? '15' : '0'; ?>%); height: 30px; bottom: 40px; background-color: #444444">
    <div id="mark-cursor"
    style="position: absolute; background-color: #0000ff; left: 0px; width: 2px; height: 30px">
</div>
<div id="mark-play"
style="position: absolute; background-color: #33aa33; left: 0px; width: 2px; height: 30px">
</div>
</div>
<div id="progressButton" class="progressButton"
style="position: absolute;background-color: #aaa; width: calc(100% - <?php echo $this->showArchive ? '15' : '0'; ?>%); height: 40px; bottom: 0px">
<div style="float: left; height: 100%; padding-left: 5px">
    <table border="0" cellpadding="0" cellspacing="0"
    style="height: 100%; margin-left: auto; margin-right: auto">
    <tr>
        <td>
            <div id="play-position" class="play-position" style="color: #dddddd"></div>
        </td>
        <td style="padding-left: 10px; padding-right: 10px">
            <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                    <td id="rec-button" class="go-button rec-button-<?php echo !$cam->record ? 'on' : 'off'; ?>">
                        Запись <?php echo $cam->record ? 'идет' : 'не идет'; ?>
                    </td>
                </tr>
            </table>
        </td>
    </tr>
</table>
</div>
<div style="float: right; height: 100%">
    <table border="0" cellpadding="0" cellspacing="0"
    style="height: 100%; padding-left: 1ex; padding-right: 0px">
    <tr>
        <td>
            <input type="text" id="pos-input-year" class="date-input year-input" placeholder="Year"/>
        </td>
        <td class="date-separator">.</td>
        <td>
            <input type="text" id="pos-input-month" class="date-input" placeholder="Mon."/>
        </td>
        <td class="date-separator">.</td>
        <td>
            <input type="text" id="pos-input-day" class="date-input" placeholder="Day"/>
        </td>
        <td>&nbsp;&nbsp;</td>
        <td>
            <input type="text" id="pos-input-hour" class="date-input" placeholder="Clock"/>
        </td>
        <td class="date-separator">:</td>
        <td>
            <input type="text" id="pos-input-minute" class="date-input" placeholder="Min."/>
        </td>
        <td class="date-separator">:</td>
        <td style="padding-right: 0px">
            <input type="text" id="pos-input-second" class="date-input" placeholder="Sec."/>
        </td>
        <td style="padding-left: 5px; padding-right: 5px">
            <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                    <td id="go-button" class="go-button">
                        Перейти
                    </td>
                </tr>
            </table>
        </td>
        <td style="padding-left: 15px; padding-right: 5px">
            <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                    <td id="download-button" class="go-button download-button">
                        Скачать
                    </td>
                </tr>
            </table>
        </td>
        <td style="padding-left: 10px; padding-right: 5px">
            <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                    <td id="live-button" class="go-button live-button">
                        Прямой эфир
                    </td>
                </tr>
            </table>
        </td>
    </tr>
</table>
</div>
<div style="float: right; height: 100%; padding-left: 1ex; padding-right: 1ex">
    <table border="0" cellpadding="0" cellspacing="0" style="height: 100%">
        <tr>
            <td>
                <div id="cursor-position" class="cursor-position"></div>
            </td>
        </tr>
    </table>
</div>
</div>
<?php } ?>
</div>
<div class="mindate" style="display:none;"></div>
<div class="maxdate" style="display:none;"></div>
<div class="downlink" style="display:none;"></div>
<?php $this->beginWidget('zii.widgets.jui.CJuiDialog',array(
    'id'=>'download-dialog',
    'options'=>array(
        'title'=>'Выбор интервала',
        'autoOpen'=>false,
        ),
    ));
echo 'Дата<br/>';
$this->widget('zii.widgets.jui.CJuiDatePicker', 
    array(
        //'model'=>$model,
        //'attribute'=>'birthdate',
        'name' => '123',
        'options'=>array(
            'showAnim'=>'fold',
            'autoSize'=>true,
            'minDate' => 'new Date(parseInt($(\'.mindate\').text()))',
            'maxDate' => 'new Date(parseInt($(\'.maxdate\').text()))',
            'dateFormat'=>'dd/mm/yy',
            'defaultDate'=>date('d/m/y'),
            ),
        'htmlOptions'=>array(
            'value'=>date_format(new DateTime, 'd/m/Y'),
            ),
        )
    ); 
    ?>
    <br/><br/>
    Промежуток времени:<br/>
    <span id="mnv">00:00:00</span>-<span id="mxv">00:00:00</span> (<span id="sumv">0</span> минут)
    <br/><br/>
    <div id="slider-range"></div>
    <br/>
    <br/>
    <input type="button" onclick="window.location.href = $('.downlink').text()" value="Скачать"><br/>
    <?php $this->endWidget(); ?>

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
    var iv = setInterval(function () {
        if ($('#<?php echo $this->showStatusbar ? $cam->id : $cam->id.'_low'; ?>').html()) {
            clearInterval(iv);
            $('#<?php echo $this->showStatusbar ? $cam->id : $cam->id.'_low'; ?>').click()
        }
    }, 100);
    $(document).ready(function() {
        $('#download-button').bind('click', function() {
            $('.downlink').text('<?php echo 'http://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_web_port'].'/mod_nvr/file?stream='.$cam->id; ?>&start='+ parseInt($('.mindate').text()/1000) + '&end=' + parseInt($('.maxdate').text()/1000));
            var t = new Date(parseInt($('.mindate').text()));
            $('#mnv').text(t.getHours() + ':' + t.getMinutes() + ':' + t.getSeconds());
            var min = t.getTime();
            t = new Date(parseInt($('.maxdate').text()));
            $('#mxv').text(t.getHours() + ':' + t.getMinutes() + ':' + t.getSeconds());
            var max = t.getTime();
            $(function() {
                $("#slider-range").slider({
                    range: true,
                    min: min,
                    max: max,
                    values: [ min, max ],
                    slide: function(event, ui) {
                        var t = new Date(parseInt(ui.values[0]));
                        $('.downlink').text('<?php echo 'http://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_web_port'].'/mod_nvr/file.mp4?stream='.$cam->id; ?>&start='+ parseInt(ui.values[0]/1000) + '&end=' + parseInt(ui.values[1]/1000));
                        $('#mnv').text(t.getHours() + ':' + t.getMinutes() + ':' + t.getSeconds());
                        t = new Date(parseInt(ui.values[1]));
                        $('#mxv').text(t.getHours() + ':' + t.getMinutes() + ':' + t.getSeconds());
                        $('#sumv').text(parseInt(((ui.values[1]-ui.values[0])/1000)/60));
                    }
                });
                    });

                $('#download-dialog').dialog("open");
            });
});
</script>
</body>
</html>
