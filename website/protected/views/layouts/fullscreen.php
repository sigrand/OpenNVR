<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link rel="stylesheet" type="text/css" href="<?php echo Yii::app()->request->baseUrl; ?>/css/bootstrap.min.css" media="all" />
	<link rel="stylesheet" type="text/css" href="<?php echo Yii::app()->request->baseUrl; ?>/css/style.css" media="all" />
	<script type="text/javascript" src="https://ajax.googleapis.com/ajax/libs/jquery/1.9.1/jquery.min.js"></script>
	<script type="text/javascript" src="<?php echo Yii::app()->request->baseUrl; ?>/js/swfobject.js"></script>
	<script type="text/javascript" src="<?php echo Yii::app()->request->baseUrl; ?>/js/bootstrap.min.js"></script>
	<title><?php echo CHtml::encode($this->pageTitle); ?></title>
	<style type="text/css">
	body {
		font-family: sans-serif;
		color: #000;
		background-color:  #fff;
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
</head>
<body style="height: calc(100%-40px); padding: 0; margin: 0">
	<div style="min-height: 100%">
		<div style="position: absolute; width: 100%; top: 0px; bottom: 70px; background-color: #000000">
			<div style="width: 100%; height: 100%; background-color: #000000">
				<div id="flash_div" style="height: 100%; padding-right: <?php echo $this->showArchive ? '15' : '0'; ?>%;">
					<?php echo $content; ?>
				</div>
			</div>
		</div>
		<?php if($this->showArchive) { ?>
		<div id="menuArchive" style="width: 15%; height: 100%; background-color: #fff; border-left: 1px solid #222222; overflow: auto; position: absolute; bottom: 0px; right: 0px;">
			<div style="padding: 1.2em; border-bottom: 5px solid #444444; vertical-align: bottom; text-align: center">
				<span style="font-size: large; font-weight: bold; color: #777777"><?php echo Yii::t('cams', 'Archive'); ?> </span>
			</div>
			<div id="menuArchiveList" style="width: 100%">
			</div>
		</div>
		<?php } ?>
		<div id="progressbar" class="progressbar" style="position: absolute; overflow: hidden; width: calc(100% - <?php echo $this->showArchive ? '15' : '0'; ?>%); height: 30px; bottom: 40px; background-color: #888">
			<div id="mark-cursor" style="position: absolute; background-color: #0000ff; left: 0px; width: 2px; height: 30px">
			</div>
			<div id="mark-play" style="position: absolute; background-color: #33aa33; left: 0px; width: 2px; height: 30px">
			</div>
		</div>
		<div id="progressButton" class="progressButton" style="position: absolute;background-color: #aaa; width: calc(100% - <?php echo $this->showArchive ? '15' : '0'; ?>%); height: 40px; bottom: 0px">
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
</body>
</html>
