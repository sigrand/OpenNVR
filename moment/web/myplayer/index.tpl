<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html style="height: 100%" xmlns="http://www.w3.org/1999/xhtml">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
  <title>NVR Server</title>
  <style type="text/css">
    body {
        font-family: sans-serif;
        color: #dddddd;
        background-color:  #222222;
    }
        body {
      font-family: sans-serif;
      color: fdddddd;
    } 

    .menuentry_one {
      background-color: #333333;
    }

    .menuentry_two {
      background-color: #222222;
    }

    div.menuentry_one:hover {
      background: #666666;
      cursor: pointer;
    }

    div.menuentry_two:hover {
      background: #666666;
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
  <link rel="stylesheet" href="./jquery-ui.css" />
  <script type="text/javascript" src="jquery.js"></script>
  <script type="text/javascript" src="url.js"></script>
  <script type="text/javascript" src="jquery-ui.js"></script>
  <script type="text/javascript">

    function togglePlaylist() {
      menuCommands = document.getElementById("menuCommands");
      menuSources = document.getElementById("menuSources");
      menuArchive = document.getElementById("menuArchive");
      flash_div = document.getElementById("flash_div");
      progressbar = document.getElementById("progressbar");
      progressButton = document.getElementById("progressButton");
      
      if (menuSources.style.display == "none") {
        menuCommands.style.display = "block";
        menuSources.style.display = "block";
        menuArchive.style.display = "block";
        flash_div.style.paddingRight = "270px";
        progressbar.style.width = "calc(100% - 270px)";        
        progressButton.style.width = "calc(100% - 270px)";
      } else {
        menuCommands.style.display = "none";
        menuSources.style.display = "none";
        menuArchive.style.display = "none";
        flash_div.style.paddingRight = "0px";
        progressbar.style.width = "100%";
        progressButton.style.width = "100%";
      }
    }
    function pad2 (a) {
        if (a < 10)
            return '0' + a;

        return '' + a;
    }
    function unixtimeToDate (unixtime) {
        var date = new Date (unixtime * 1000);
        var date_str = date.getFullYear() + '.'
                       + (date.getMonth() + 1) + '.'
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
        play_start_pos = cur_position_playing_archive;
        last_time_delta = 0;
        setPlayPosition (0);
    }

    function setPlayPosition (time_delta) {
        if (isNaN (time_delta))
            time_delta = last_time_delta;
        else
            last_time_delta = time_delta;
        play_position_sel.html (unixtimeToDate (play_start_pos + time_delta));

        var z = end_playing_archive - start_playing_archive;        
        var y = $(".progressbar").width();
        var x = time_delta;

        if (cur_position_playing_archive == 0)
          var g = 0;
        else
          var g = cur_position_playing_archive - start_playing_archive
        $("#mark-play")[0].style.left = ((g+x) * (y/z)) + 'px';
        //console.log('play_start_pos' + play_start_pos + ' <-reference_pos' + reference_pos + '<-' + start_playing_archive + ' ' + end_playing_archive);
    }


    var recording_on = false;
    function showRecording () {
        if (recording_on) {
            $("#rec-button").html ("REC OFF");
            $("#rec-button").attr ("class", "go-button rec-button-on");
        } else {
            $("#rec-button").html ("REC ON");
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
        $.get ("/mod_nvr/channel_state?stream=Sigrand_1C-131&seq=" + state_seq,
            {}, processStateReply);
    }

    function doDownload (download) {
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

    function updatePlaylist(){
      $.ajax({
        cache: false,
        type: "GET",
        url: "/server/playlist.json",
        contentType: "application/json",
        dataType: "json",
        data: null,
        crossDomain: true,
        error: function(jqXHR, ajaxOptions, thrownError) {
          console.log('Error: ' + jqXHR + ' | ' + jqXHR);
          console.log('Error: ' + ajaxOptions + ' | ' + thrownError + ' | ' + jqXHR);
        },
        success: function(data) {
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
            entrySource.style.verticalAlign = "bottom";
            entrySource.onclick = (function (uri, stream_name, index, entrySource) {
              return function () {
                $.ajax({
                  cache: false,
                  type: "GET",
                  url: "/mod_nvr/existence?stream=" + stream_name,
                  contentType: "application/json",
                  dataType: "json",
                  data: null,
                  crossDomain: true,
                  error: function(jqXHR) {
                    console.log('Error: ' + jqXHR.status + ' ' + jqXHR.statusText);
                      var children = document.getElementById('menuSourcesList').childNodes;
                      for(var i = 0; i < children.length; ++i)
                      {
                        children[i].style.backgroundColor = (i % 2 === 0)? "#333333": "#222222";
                      }

                      cur_playing = index;
                      console.log("YYYY cur_playing is " + cur_playing);
                      entrySource.style.backgroundColor = "#666666";
                  },
                  success: function(data) {
                    if (data && data["timings"]) {
                      while (menuArchiveList.hasChildNodes()) {
                        menuArchiveList.removeChild(menuArchiveList.lastChild);
                      }

                      var children = document.getElementById('menuSourcesList').childNodes;
                      for(var i = 0; i < children.length; ++i)
                      {
                        children[i].style.backgroundColor = (i % 2 === 0)? "#333333": "#222222";
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

                              console.log('(setSourceSeeked).click  ' +  uriParsered.protocol + uriParsered.host + uriParsered.port  + stream_name + time_start +' '+  time_end);

                              start_playing_archive = time_start;
                              cur_position_playing_archive = time_start;
                              end_playing_archive = time_end;

                              cur_playing_archive = index;
                              console.info(start_playing_archive + "<- start and stop -> " + end_playing_archive);
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
                                    function (dataTime) {
                                        var unixtime = eval ('(' + dataTime + ')');
                                        var local_unixtime = Math.floor ((new Date).getTime() / 1000);

                                        /*var date = new Date (unixtime * 1000);*/

                                        /* var unixtime = Math.floor (date.getTime() / 1000); */

                                        reference_pos = data["timings"][cur_playing_archive]["start"];
                                        console.log( cur_playing_archive + " do function cur_playing_archive");
                                        setPlayStartPosition (unixtime, true);

                                        $("#progressbar").click (function (e) {

                                            var z =(data["timings"][cur_playing_archive]["end"] - data["timings"][cur_playing_archive]["start"]);                                            
                                            var y = $(".progressbar").width();
                                            var x = e.pageX;
                                            var time = Math.floor (reference_pos + (x * (z/y)));
                                            
                                            setPlayStartPosition (time, true); 

                                            time_end = data["timings"][cur_playing_archive]["end"];

                                            document["MyPlayer"].setSourceSeeked(uriParsered.protocol + "://" + uriParsered.host + ":" + uriParsered.port + "/nvr/" + stream_name, stream_name, time, time_end);

                                            cur_position_playing_archive = time;
                                        });

                                        $("#progressbar").mousemove (function (e) {
                                            
                                            var z =(data["timings"][cur_playing_archive]["end"] - data["timings"][cur_playing_archive]["start"]);                                            
                                            var y = $(".progressbar").width();
                                            var x = e.pageX;
                                            //console.log(e.pageX + " e.pageX " + z +  "  " + y + ' ' + x);
                                            var time = Math.floor (reference_pos + (x * (z/y)));
                                            mark_cursor_x = x;
                                            mark_cursor_sel.style.left = mark_cursor_x + 'px';
                                            setCursorPosition (time);
                                        });

                                        $("#go-button").click (function (e) {
                                            var time = getPosInputUnixtime();
                                            mark_cursor_sel.style.left = (mark_cursor_x + reference_pos - time) + 'px';
                                            reference_pos = time;
                                            setPlayStartPosition (time, true);
                                            document["MyPlayer"].setSourceSeeked(uriParsered.protocol + "://" + uriParsered.host + ":" + uriParsered.port + "/nvr/" + stream_name, stream_name, time, time_end); 
                                        });

                                        $("#download-button").click (function (e) {
                                            doDownload (true);
                                        });

                                        $("#watch-button").click (function (e) {
                                            doDownload (false);
                                        });

                                        $("#live-button").click (function (e) {
                                            var time = getPosInputUnixtime();
                                            document ["MyPlayer"].setSource (uri, stream_name + "?start=" + time);
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
                                            $.get ("/mod_nvr_admin/rec_" + (was_on ? "off" : "on") + "?stream=Sigrand_1C-131&seq=" + state_seq,
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
                console.log("stream_name - ", stream_name );
                document["MyPlayer"].setSource (uri, stream_name );
              };
            })(item["uri"], item["name"], i, entrySource);
            entrySource.innerHTML = item["title"];
            menuSourcesList.appendChild(entrySource);
          }
          first_uri         = playlist[0]["uri"];
          first_stream_name = playlist[0]["name"];
          if (first_uri && flash_initialized) {
            document["MyPlayer"].setFirstUri(first_uri, first_stream_name);
          }
        }
      });
    }

    function doResize () {
        //$("#mark-middle")[0].style.left = /*Math.floor ($(window).width() / 2)*/  0  + 'px';
        $("#mark-play")[0].style.left = Math.floor (last_time_delta)  + 'px';
    }

  </script>
</head>
<body style="height: calc(100%-40px); padding: 0; margin: 0">
  <div style="min-height: 100%">
    <div style="position: absolute; width: 100%; top: 0px; bottom: 70px; background-color: #000000">
      <div style="width: 100%; height: 100%; background-color: #000000">
        <div id="flash_div" style="height: 100%; padding-right: 270px;">
          <object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" width="100%" height="100%" id="MyPlayer" align="middle">
            <param name="movie" value="MyPlayer.swf"/>
            <param name="allowScriptAccess" value="always"/>
            <param name="quality" value="high"/>
            <param name="scale" value="noscale"/>
            <param name="salign" value="lt"/>
            <param name="bgcolor" value="#000000"/>
            <param name="allowFullScreen" value="true"/>
            <param name="FlashVars" value="autoplay=0&playlist=1&buffer=2.0"/>
            <embed FlashVars="autoplay=0&playlist=1&buffer=2.0"
              src="MyPlayer.swf"
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
            <!-- FIXME: scale="showall" may cause scaling problems -->
          </object>
        </div>
      </div>
    </div>

    <div id="menuCommands" style="width: 270px; height: 50%; background-color: #111111; border-left: 1px solid #222222; overflow: auto; position: absolute; top: 0px; right: 0px; height: 20%">
      <div style="padding: 1.2em; border-bottom: 5px solid #444444; vertical-align: bottom; text-align: center">
        <span style="font-size: large; font-weight: bold; color: #777777">Commands</span>
      </div>
      <div id="menuCommandsList" style="width: 100%">
      </div>
    </div>
    <div id="menuSources" style="width: 270px; height: 50%; background-color: #111111; border-left: 1px solid #222222; overflow: auto; position: absolute; top: 20%; right: 0px; height: 40%">
      
      <div style="padding: 1.2em; border-bottom: 5px solid #444444; vertical-align: bottom; text-align: center">
        <span style="font-size: large; font-weight: bold; color: #777777">Sources</span>
      </div>
      
      <div id="menuSourcesList" style="width: 100%">
      </div>
    </div>
    <div id="menuArchive" style="width: 270px; height: 50%; background-color: #111111; border-left: 1px solid #222222; overflow: auto; position: absolute; bottom: 0px; right: 0px; height: 40%">
      <div style="padding: 1.2em; border-bottom: 5px solid #444444; vertical-align: bottom; text-align: center">
        <span style="font-size: large; font-weight: bold; color: #777777">Archive</span>
      </div>
      <div id="menuArchiveList" style="width: 100%">
      </div>
    </div>
    <div id="dialogAdd">
      <div>
        <label >Name</label>
        <textarea id="name" style="height: 20px; width: 100%"></textarea>
      </div>
      <div>
          <label>Uri</label>
        <textarea id="uri" style="height: 20px; width: 100%"></textarea>
      </div>
      <div>
          <label>Title</label>
          <textarea id="title" style="height: 20px; width: 100%"></textarea>
      </div>
    </div>
    <div id="dialogModify">
      <div>
          <label>Uri</label>
        <textarea id="uriMod" style="height: 20px; width: 100%"></textarea>
      </div>
      <div>
          <label>Title</label>
          <textarea id="titleMod" style="height: 20px; width: 100%"></textarea>
      </div>
    </div>
    <div id="dialogRemove">
      <label></label>
    </div>

    <div id="progressbar" class="progressbar" style="position: absolute; overflow: hidden; width: calc(100% - 270px); height: 30px; bottom: 40px; background-color: #444444">
      <div id="mark-cursor" style="position: absolute; background-color: #0000ff; left: 0px; width: 2px; height: 30px">
      </div>
      <div id="mark-play" style="position: absolute; background-color: #33aa33; left: 0px; width: 2px; height: 30px">
      </div>
    </div>
    <div id="progressButton" class="progressButton" style="position: absolute;background-color: #111111; width: calc(100% - 270px); height: 40px; bottom: 0px">
      <div style="float: left; height: 100%; padding-left: 5px">
        <table border="0" cellpadding="0" cellspacing="0" style="height: 100%; margin-left: auto; margin-right: auto">
          <tr>
            <td>
              <div id="play-position" class="play-position"  style="color: #dddddd"></div>
            </td>
            <td style="padding-left: 10px; padding-right: 10px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="rec-button" class="go-button rec-button-off">
                    REC ON
                  </td>
                </tr>
              </table>
            </td>
          </tr>
        </table>
      </div>
      <div style="float: right; height: 100%">
        <table border="0" cellpadding="0" cellspacing="0" style="height: 100%; padding-left: 1ex; padding-right: 0px">
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
                    GO TO
                  </td>
                </tr>
              </table>
            </td>
            <td style="padding-left: 15px; padding-right: 5px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="download-button" class="go-button download-button">
                    Download
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
            <td style="color: gray; padding-right: 5px">min.</td>
            <td style="padding-left: 10px; padding-right: 5px">
              <table border="0" cellpadding="0" cellspacing="0" style="height: 28px">
                <tr>
                  <td id="live-button" class="go-button live-button">
                    Show now
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

    $("#dialogAdd").dialog({
        autoOpen: false,
        modal: true,
        resizable: false,
        maxWidth:400,
        maxHeight: 300,
        width: 400,
        height: 300,
        buttons: { 
            Ok: function() {

                var the_name = encodeURIComponent($("#name").val());
                var the_uri = encodeURIComponent($("#uri").val());
                var the_title = encodeURIComponent($("#title").val());
                var update = (isModify === true)? "&update": "";

                $.ajax({
                  cache: false,
                type: "GET",
                url: "http://localhost:8082/admin/add_channel?conf_file=" + the_name + "&title=" + the_title + "&uri=" + the_uri + update,
                contentType: "application/json",
                dataType: "json",
                data: null,
                crossDomain: true,
                  error: function(jqXHR, ajaxOptions, thrownError) {
                    console.log('Error: ' + jqXHR.status + ' | ' + jqXHR.statusText);
                    console.log('Error: ' + ajaxOptions + ' | ' + thrownError + ' | ' + jqXHR.responseText);
                    updatePlaylist();
                  },
                  success: function(data) {
                    if (isModify === true)
                    {
                      alert("succeed add");
                    }
                    else
                    {
                      alert("succeed modify");
                    }
                    
                    updatePlaylist();
                  }
                });

                $(this).dialog("close");
            },
            Cancel: function () {
            $(this).dialog("close");
            }
        }
    });

    $("#dialogModify").dialog({
        autoOpen: false,
        modal: true,
        resizable: false,
        maxWidth:400,
        maxHeight: 300,
        width: 400,
        height: 300,
        buttons: { 
            Ok: function() {
                //http://192.168.10.21:8082/admin/add_channel?conf_file=sony6&title=Sony6&uri=rtsp://192.168.10.21:8092/test0.sdp
              var the_name = "";
              $.get("/server/playlist.json", {}, function (data) {
                var playlist = data["sources"];
                var the_name = playlist[cur_playing]["name"];
        var the_uri = encodeURIComponent($("#uriMod").val());
        var the_title = encodeURIComponent($("#titleMod").val());
        var update = (isModify === true)? "&update=true": "";

        console.log("1 " + the_name);
        console.log("2 " + the_uri);
        console.log("3 " + the_title);

                  $.ajax({
                    cache: false,
              type: "GET",
              url: "http://localhost:8082/admin/add_channel?conf_file=" + the_name + "&title=" + the_title + "&uri=" + the_uri + update,
              contentType: "application/json",
              dataType: "json",
              data: null,
              crossDomain: true,
              error: function(jqXHR, ajaxOptions, thrownError) {
                console.log('Error: ' + jqXHR.status + ' | ' + jqXHR.statusText);
                console.log('Error: ' + ajaxOptions + ' | ' + thrownError + ' | ' + jqXHR.responseText);
                updatePlaylist();
              },
              success: function(data) {
                if (isModify === true)
                {
                  alert("succeed add");
                }
                else
                {
                  alert("succeed modify");
                }
                
                updatePlaylist();
              }
                  });
              });



                $(this).dialog("close");
            },
            Cancel: function () {
                $(this).dialog("close");
            }
        }
    });

    $("#dialogRemove").dialog({
        autoOpen: false,
        modal: true,
        resizable: false,
        maxWidth:275,
        maxHeight: 200,
        width: 275,
        height: 200,
        buttons: { 
            Ok: function() {
              var the_name = "";
              $.get("/server/playlist.json", {}, function (data) {
                var playlist = data["sources"];

            the_name = playlist[cur_playing]["name"];
            cur_playing = -1;
        
            var menuArchiveList = document.getElementById("menuArchiveList");

            while (menuArchiveList.hasChildNodes()) {
              menuArchiveList.removeChild(menuArchiveList.lastChild);
            }

                  $.ajax({
                    cache: false,
              type: "GET",
              url: "http://localhost:8082/admin/remove_channel?conf_file=" + the_name,
              contentType: "application/json",
              dataType: "json",
              data: null,
              crossDomain: true,
              error: function(jqXHR, ajaxOptions, thrownError) {
                updatePlaylist(); //wrong decision
              },
              success: function(data) {
                if (isModify === true)
                {
                  alert("succeed add");
                }
                else
                {
                  alert("succeed modify");
                }
                
                updatePlaylist();
              }
                  });
              });

                $(this).dialog("close");
            },
            Cancel: function () {
                $(this).dialog("close");
            }
        }
    });

    var menuCommandsList = document.getElementById("menuCommandsList");
    var commands = ["Add", "Modify", "Remove"];
    var class_one = "menuentry_one";
    var class_two = "menuentry_two";

    // Add element
    var entrySourceAdd = document.createElement("div");

    entrySourceAdd.className = class_one;
    entrySourceAdd.style.padding = "10px";
    entrySourceAdd.style.textAlign = "left";
    entrySourceAdd.style.verticalAlign = "bottom";
    entrySourceAdd.innerHTML = commands[0];
    entrySourceAdd.onclick = function myFunction(){ isModify=false; $("#dialogAdd").dialog("open");}

    menuCommandsList.appendChild(entrySourceAdd);

    // Modify element
    var entrySourceModify = document.createElement("div");

    entrySourceModify.className = class_two;
    entrySourceModify.style.padding = "10px";
    entrySourceModify.style.textAlign = "left";
    entrySourceModify.style.verticalAlign = "bottom";
    entrySourceModify.innerHTML = commands[1];
    entrySourceModify.onclick = function myFunction(){
      if(cur_playing === -1)
        return;
      isModify=true; 
      $.get("/server/playlist.json", {}, function (data) {
        var playlist = data["sources"];
        console.log('server/playlist.json' + playlist[cur_playing]["uriSrc"]);

        $("#uriMod").val(playlist[cur_playing]["uriSrc"]);
        $("#titleMod").val(playlist[cur_playing]["title"]);
      });
      $("#dialogModify").dialog("open");
    }

    menuCommandsList.appendChild(entrySourceModify);

    // Remove element
    var entrySourceRemove = document.createElement("div");

    entrySourceRemove.className = class_one;
    entrySourceRemove.style.padding = "10px";
    entrySourceRemove.style.textAlign = "left";
    entrySourceRemove.style.verticalAlign = "bottom";
    entrySourceRemove.innerHTML = commands[2];
    entrySourceRemove.onclick = function myFunction(){
      $.get("/server/playlist.json", {}, function (data) {
        var playlist = data["sources"];
        $("#dialogRemove").text("Are you sure to delete this source \"" + playlist[cur_playing]["title"] + "\"?");
        $("#dialogRemove").dialog("open");      
      });
    }
    
    menuCommandsList.appendChild(entrySourceRemove);
    var menuSourcesList = document.getElementById("menuSourcesList");
    var menuArchiveList = document.getElementById("menuArchiveList");

  </script>
</body>
</html>
