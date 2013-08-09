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
    }
  </style>
</head>
<body style="height: 100%; padding: 0; margin: 0; background-color: #000000">
  <div style="width: 100%; height: 100%; background-color: #000000;">
    <div id="flash_div" style="height: 100%">
      <object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000"
	      width="100%"
	      height="100%"
	      id="MyPlayer"
	      align="middle">
	<param name="movie" value="MyPlayer.swf"/>
	<param name="allowScriptAccess" value="always"/>
	<param name="quality" value="high"/>
	<param name="scale" value="noscale"/>
	<param name="salign" value="lt"/>
	<param name="bgcolor" value="#000000"/>
	<param name="allowFullScreen" value="true"/>
	<param name="FlashVars" value="autoplay=1&playlist=0&buffer=2.0&uri=rtmpt://{{ThisRtmptServerAddr}}/live/&stream=test"/>
	<embed              FlashVars="autoplay=1&playlist=0&buffer=2.0&uri=rtmpt://{{ThisRtmptServerAddr}}/live/&stream=test"
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
      </object>
    </div>
  </div>
</body>
</html>

