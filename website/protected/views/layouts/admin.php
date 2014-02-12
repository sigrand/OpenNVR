<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link rel="stylesheet" type="text/css" href="<?php echo Yii::app()->request->baseUrl; ?>/css/bootstrap.min.css" media="all" />            
	<link rel="stylesheet" type="text/css" href="<?php echo Yii::app()->request->baseUrl; ?>/css/style.css" media="all" />                          
	<script type="text/javascript" src="https://ajax.googleapis.com/ajax/libs/jquery/1.9.1/jquery.min.js"></script>
	<script type="text/javascript" src="<?php echo Yii::app()->request->baseUrl; ?>/js/bootstrap.min.js"></script>   
	<title><?php echo CHtml::encode($this->pageTitle); ?></title>
</head>
<body>
	<nav class="navbar navbar-default navbar-inverse" role="navigation">
		<div class="container">
			<div class="navbar-header">
				<button type="button" class="navbar-toggle" data-toggle="collapse" data-target="#bs-example-navbar-collapse-1">
					<span class="sr-only">Toggle navigation</span>
					<span class="icon-bar"></span>
					<span class="icon-bar"></span>
					<span class="icon-bar"></span>
				</button>
			</div>
			<div class="collapse navbar-collapse" id="bs-example-navbar-collapse-1">
				<ul class="nav navbar-nav">
					<li><?php echo CHtml::link(Yii::t('menu', 'Главная'), $this->createUrl('admin/index')); ?></li>
					<li><?php echo CHtml::link(Yii::t('menu', 'Статистика сервера'), $this->createUrl('admin/stat', array('type' => 'disk'))); ?></li>
					<li><?php echo CHtml::link(Yii::t('menu', 'Камеры'), $this->createUrl('admin/cams')); ?></li>
					<li><?php echo CHtml::link(Yii::t('menu', 'Пользователи'), $this->createUrl('admin/users')); ?></li>
					<li><?php echo CHtml::link(Yii::t('menu', 'Логи'), $this->createUrl('admin/logs', array('type' => 'system'))); ?></li>
				</ul>
				<ul class="nav navbar-nav navbar-right">
					<li><?php echo CHtml::link(Yii::t('menu', 'На сайт'), $this->createUrl('/')); ?></li>
				</ul>
			</div>
		</div>
	</nav>
	<div class="container">
		<?php echo $content; ?>
	</div>
</body>
</html>
