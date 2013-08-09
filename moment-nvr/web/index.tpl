<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html style="height: 100%" xmlns="http://www.w3.org/1999/xhtml">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
  <title>Moment Video Server - http://momentvideo.org</title>
  <link rel="icon" type="image/vnd.microsoft.icon" href="favicon.ico"/>
  <style type="text/css">
    body {
        font-family: sans-serif;
        color: #000000;
        background-color: /* #bbffbb */ #ddddff;
    }

    .play-position {
        font-size: larger;
        color: /* #006600 */ #333333;
    }

    .cursor-position {
        font-size: larger;
        color: #000088;
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
  <script type="text/javascript" src="jquery.js"></script>
  <script type="text/javascript">
    function pad2 (a) {
        if (a < 10)
            return '0' + a;

        return '' + a;
    }

    function unixtimeToDate (unixtime) {
        var date = new Date (unixtime * 1000);
        var date_str = date.getFullYear() + '/'
                       + (date.getMonth() + 1) + '/'
                       + date.getDate() + ' '
                       + pad2 (date.getHours()) + ':'
                       + pad2 (date.getMinutes()) + ':'
                       + pad2 (date.getSeconds());
        return date_str;
    }

    var cursor_position_sel;
    var play_position_sel;
    var reference_pos = 0;
    var play_start_pos = 0;
    var last_time_delta = 0;

    function getPosInputUnixtime ()
    {
        var date = new Date ($("#pos-input-year").val(),
                             $("#pos-input-month").val() - 1,
                             $("#pos-input-day").val(),
                             $("#pos-input-hour").val(),
                             $("#pos-input-minute").val(),
                             $("#pos-input-second").val(),
                             0);
        var time = Math.floor (date.getTime() / 1000);
        return time;
    }

    function setCursorPosition (unixtime) {
        cursor_position_sel.html (unixtimeToDate (unixtime));
    }

    function setPlayStartPosition (unixtime, set_input_pos) {
        if (set_input_pos) {
            var date = new Date (unixtime * 1000);
            $("#pos-input-year").val (date.getFullYear());
            $("#pos-input-month").val (date.getMonth() + 1);
            $("#pos-input-day").val (date.getDate());
            $("#pos-input-hour").val (pad2 (date.getHours()));
            $("#pos-input-minute").val (pad2 (date.getMinutes()));
            $("#pos-input-second").val (pad2 (date.getSeconds()));
        }

        play_start_pos = unixtime;
        last_time_delta = 0;
        setPlayPosition (0);
    }

    function setPlayPosition (time_delta) {
        if (isNaN (time_delta))
            time_delta = last_time_delta;
        else
            last_time_delta = time_delta;

        play_position_sel.html (unixtimeToDate (play_start_pos + time_delta));
        $("#mark-play")[0].style.left = Math.floor ($(window).width() / 2 + (play_start_pos - reference_pos + time_delta)) + 'px';
    }

    var recording_on = false;
    function showRecording () {
        if (recording_on) {
            $("#rec-button").html ("Выключить запись");
            $("#rec-button").attr ("class", "go-button rec-button-on");
        } else {
            $("#rec-button").html ("Включить запись");
            $("#rec-button").attr ("class", "go-button rec-button-off");
        }
    }

    function processStateReply (reply) {
        state = $.parseJSON (reply);
        if (state.seq != state_seq)
            return;

        recording_on = state.recording;
        showRecording ();
    }

    var state_seq = 0;
    function requestState () {
        state_seq = state_seq + 1;
        $.get ("/mod_nvr/channel_state?stream={{NvrStreamName}}&seq=" + state_seq,
            {}, processStateReply);
    }

    $(document).ready (function() {
        cursor_position_sel = $("#cursor-position");
        play_position_sel = $("#play-position");
        var mark_cursor_sel = $("#mark-cursor")[0];
        var mark_cursor_x = 0;

        $(window).resize (function () {
            doResize ();
        });

        doResize ();

        /* TODO POST */
        $.get ("/mod_nvr/unixtime",
            {},
            function (data) {
                var unixtime = eval ('(' + data + ')');
                var local_unixtime = Math.floor ((new Date).getTime() / 1000);

                /*var date = new Date (unixtime * 1000);*/

                /* var unixtime = Math.floor (date.getTime() / 1000); */
                reference_pos = unixtime;
                setPlayStartPosition (unixtime, true);

                /*
                $("#pos-input-year").val (date.getFullYear());
                $("#pos-input-month").val (date.getMonth() + 1);
                $("#pos-input-day").val (date.getDate());
                $("#pos-input-hour").val (pad2 (date.getHours()));
                $("#pos-input-minute").val (pad2 (date.getMinutes()));
                $("#pos-input-second").val (pad2 (date.getSeconds()));
                */

                $("#progressbar").click (function (e) {
                    var x = e.pageX - this.offsetLeft;
                    var y = e.pageY - this.offsetTop;
                    var time = Math.floor (reference_pos - ($(window).width() / 2 - x));
                    setPlayStartPosition (time, true);
                    document ["MyPlayer"].setSource ("rtmp://{{ThisRtmpServerAddr}}/nvr/", "{{NvrStreamName}}?start=" + time);
                });

                $("#progressbar").mousemove (function (e) {
                    var x = e.pageX - this.offsetLeft;
                    var y = e.pageY - this.offsetTop;
                    var time = reference_pos - (Math.floor ($(window).width() / 2) - x);
                    mark_cursor_x = x;
                    mark_cursor_sel.style.left = x + 'px';
                    setCursorPosition (time);
                });

                $("#go-button").click (function (e) {
                    var time = getPosInputUnixtime();
                    mark_cursor_sel.style.left = (mark_cursor_x + reference_pos - time) + 'px';
                    reference_pos = time;
                    setPlayStartPosition (time, true);
                    document ["MyPlayer"].setSource ("rtmp://{{ThisRtmpServerAddr}}/nvr/", "{{NvrStreamName}}?start=" + time);
                });

                function doDownload (download) {
                    var duration = $("#download-minutes").val() * 60;
                    if (duration == 0)
                        duration = 60;
                    var link =  "http://{{ThisHttpServerAddr}}/mod_nvr/file.mp4?stream={{NvrStreamName}}&start="
                                    + getPosInputUnixtime()
                                    + "&duration=" + duration + (download ? "&download" : "");
                    if (download)
                        window.location.href = link;
                    else
                        window.open (link);
                }

                $("#download-button").click (function (e) {
                    doDownload (true);
                });

                $("#watch-button").click (function (e) {
                    doDownload (false);
                });

                $("#live-button").click (function (e) {
                    var cur_local_unixtime = Math.floor ((new Date).getTime() / 1000);
                    setPlayStartPosition (unixtime + (cur_local_unixtime - local_unixtime), false);
                    document ["MyPlayer"].setSource ("rtmp://{{ThisRtmpServerAddr}}", "{{NvrStreamName}}");
                });

                setInterval (
                    function () {
                        setPlayPosition (document["MyPlayer"].getPlayheadTime());
                    },
                    1000);

                requestState ();
                setInterval (
                    function () { requestState (); },
                    1000);

                $("#rec-button").click (function (e) {
                    var was_on = recording_on;

                    recording_on = !recording_on;
                    showRecording ();

                    state_seq = state_seq + 1;
                    $.get ("http://{{ThisAdminServerAddr}}/mod_nvr_admin/rec_" + (was_on ? "off" : "on") + "?stream={{NvrStreamName}}&seq=" + state_seq,
                        {}, processStateReply);
                });
            }
        );
    });

    function flashInitialized ()
    {
        document ["MyPlayer"].setSource ("rtmp://{{ThisRtmpServerAddr}}", "{{NvrStreamName}}");
    }

    function doResize () {
        $("#mark-middle")[0].style.left = Math.floor ($(window).width() / 2) + 'px';
        $("#mark-play")[0].style.left = Math.floor ($(window).width() / 2 + last_time_delta) + 'px';
    }
  </script>
</head>
<body style="height: 100%; padding: 0; margin: 0">
  <div style="min-height: 100%">
    <div style="position: absolute; width: 100%; top: 0px; bottom: 80px; background-color: #aaffaa">
      <div style="width: 100%; height: 100%; background-color: #000000">
        <div id="flash_div" style="height: 100%">
          <object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000"
                  width="100%"
                  height="100%"
                  id="MyPlayer"
                  align="middle">
            <param name="movie" value="MyPlayer.swf?1"/>
            <param name="allowScriptAccess" value="always"/>
            <param name="quality" value="high"/>
            <param name="scale" value="noscale"/>
            <param name="salign" value="lt"/>
            <param name="bgcolor" value="#000000"/>
            <param name="allowFullScreen" value="true"/>
            <param name="FlashVars" value="autoplay=0&playlist=0&buffer={{NvrPlayerBuffer}}"/>
            <embed              FlashVars="autoplay=0&playlist=0&buffer={{NvrPlayerBuffer}}"
                src="MyPlayer.swf?1"
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
      </div>
    </div>
    <div id="progressbar" class="progressbar" style="position: absolute; overflow: hidden; width: 100%; height: 40px; bottom: 40px; background-color: #ffffbb">
      <div id="mark-middle" style="position: absolute; background-color: #55aadd; left: 0px; width: 2px; height: 40px">
      </div>
      <div id="mark-cursor" style="position: absolute; background-color: #0000ff; left: 0px; width: 2px; height: 40px">
      </div>
      <div id="mark-play" style="position: absolute; background-color: #33aa33; left: 0px; width: 2px; height: 40px">
      </div>
    </div>
    <div style="position: absolute; width: 100%; height: 40px; bottom: 0px">
      <div style="float: left; height: 100%; padding-left: 5px">
        <table border="0" cellpadding="0" cellspacing="0" style="height: 100%; margin-left: auto; margin-right: auto">
          <tr>
            <td>
              <div id="play-position" class="play-position"></div>
            </td>
            <td style="padding-left: 10px; padding-right: 10px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="rec-button" class="go-button rec-button-off">
                    Включить запись
                  </td>
                </tr>
              </table>
            </td>
            <!--
            <td>
              <div id="debug" style="padding-left: 1em">DEBUG</div>
            </td>
            -->
          </tr>
        </table>
      </div>
      <div style="float: right; height: 100%">
        <table border="0" cellpadding="0" cellspacing="0" style="height: 100%; padding-left: 1ex; padding-right: 0px">
          <tr>
            <td>
              <input type="text" id="pos-input-year" class="date-input year-input" placeholder="Год"/>
            </td>
            <td class="date-separator">/</td>
            <td>
              <input type="text" id="pos-input-month" class="date-input" placeholder="Мес"/>
            </td>
            <td class="date-separator">/</td>
            <td>
              <input type="text" id="pos-input-day" class="date-input" placeholder="Ден"/>
            </td>
            <td>&nbsp;&nbsp;</td>
            <td>
              <input type="text" id="pos-input-hour" class="date-input" placeholder="Час"/>
            </td>
            <td class="date-separator">:</td>
            <td>
              <input type="text" id="pos-input-minute" class="date-input" placeholder="Мин"/>
            </td>
            <td class="date-separator">:</td>
            <td style="padding-right: 0px">
              <input type="text" id="pos-input-second" class="date-input" placeholder="Сек"/>
            </td>
            <td style="padding-left: 5px; padding-right: 5px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="go-button" class="go-button">
                    Переход
                  </td>
                </tr>
              </table>
            </td>
            <td style="padding-left: 15px; padding-right: 5px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="download-button" class="go-button download-button">
                    Загрузить
                  </td>
                </tr>
              </table>
            </td>
            <td style="padding-left: 0px; padding-right: 5px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="watch-button" class="go-button watch-button">
                    <b>w</b>
                  </td>
                </tr>
              </table>
            </td>
            <td style="padding-right: 5px">
              <input type="text" id="download-minutes" class="date-input" value="5"/>
            </td>
            <td style="color: gray; padding-right: 5px">мин.</td>
            <td style="padding-left: 10px; padding-right: 5px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="live-button" class="go-button live-button">
                    Сейчас
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
  </div>
</body>
</html>

