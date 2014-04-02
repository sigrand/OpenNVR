<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link rel="stylesheet" type="text/css" href="<?php echo Yii::app()->request->baseUrl; ?>/css/bootstrap.min.css" media="all" />            
	<link rel="stylesheet" type="text/css" href="<?php echo Yii::app()->request->baseUrl; ?>/css/style-<?php echo Settings::model()->getValue('style'); ?>.css" media="all" />                          
	<script type="text/javascript" src="https://ajax.googleapis.com/ajax/libs/jquery/1.9.1/jquery.min.js"></script>
	<script type="text/javascript" src="<?php echo Yii::app()->request->baseUrl; ?>/js/bootstrap.min.js"></script>   
	<title><?php echo CHtml::encode($this->pageTitle); ?></title>
</head>
<body>
	<nav class="navbar navbar-default navbar-inverse" role="navigation" style="z-index:10000">
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
					<li><?php echo CHtml::link(Yii::t('menu', 'Main'), $this->createUrl('/'), array("id" => "logo")); ?></li>
				</ul>
				<ul class="nav navbar-nav navbar-right">
					<li>
						<?php 
						$this->widget('application.components.widgets.LanguageSelector');
						?>
					</li>
					<?php if(!Yii::app()->user->isGuest) { ?>
					<?php if(Yii::app()->user->isAdmin) { ?>
					<li class="dropdown">
						<a href="#" class="dropdown-toggle" data-toggle="dropdown"><?php echo Yii::t('menu', 'Admin'); ?> <b class="caret"></b></a>
						<ul class="dropdown-menu">
							<li><?php echo CHtml::link(Yii::t('menu', 'Servers'), $this->createUrl('admin/servers')); ?></li>
							<li><?php echo CHtml::link(Yii::t('menu', 'Public cams'), $this->createUrl('admin/cams')); ?></li>
							<li><?php echo CHtml::link(Yii::t('menu', 'Users'), $this->createUrl('admin/users')); ?></li>
							<li><?php echo CHtml::link(Yii::t('menu', 'Logs'), $this->createUrl('admin/logs', array('type' => 'system'))); ?></li>
							<li><?php echo CHtml::link(Yii::t('menu', 'Settings'), $this->createUrl('admin/settings')); ?></li>
							<li><?php echo CHtml::link(Yii::t('menu', 'Updater'), $this->createUrl('admin/updater')); ?></li>
						</ul>
					</li>
					<?php } ?>
					<li>
						<a href="<?php echo $this->createUrl('users/notifications'); ?>">
							<?php echo Yii::t('menu', 'Notifications'); ?>
							<span class="badge"><?php echo Notify::model()->countByAttributes(array('dest_id' => Yii::app()->user->getId(), 'status' => 1)); ?></span>
						</a>
					</li>
					<?php if((Yii::app()->user->permissions == 2) || (Yii::app()->user->permissions == 3)) { ?>
					<li><?php echo CHtml::link(Yii::t('menu', 'Cams'), $this->createUrl('cams/manage')); ?></li>
					<?php } ?>
					<li class="dropdown">
						<a href="#" class="dropdown-toggle" data-toggle="dropdown"><?php echo Yii::t('menu', 'Screens'); ?> <b class="caret"></b></a>
						<ul class="dropdown-menu">
							<li><?php echo CHtml::link(Yii::t('menu', 'Manage'), $this->createUrl('screens/manage')); ?></li>
							<?php
							if (Yii::app()->user->permissions == 3) {
								$myscreens = Screens::model()->findAll();
							} else {
								$myscreens = Screens::model()->findAllByAttributes(array('owner_id' => Yii::app()->user->getId()));
							}
							foreach ($myscreens as $key => $value) {
								echo "<li>".CHtml::link("$value->name", $this->createUrl("screens/view/id/$value->id"))."</li>";
							}
							?>
						</ul>
					</li>

					<li><?php echo CHtml::link(Yii::t('menu', 'Account settings'), $this->createUrl('users/profile', array('id' => 'any'))); ?></li>
					<li><?php echo CHtml::link(Yii::t('menu', 'Logout [{nick}]', array('{nick}' => Yii::app()->user->nick)), $this->createUrl('site/logout')); ?></li>
					<?php } else { ?>
					<li><?php echo CHtml::link(Yii::t('menu', 'Login'), $this->createUrl('site/login')); ?></li>
					<li><?php echo CHtml::link(Yii::t('menu', 'Register'), $this->createUrl('site/register')); ?></li>
					<?php } ?>
				</ul>
			</div>
		</div>
	</nav>
	<div class="container">
		<?php echo $content; ?>
	</div>
</body>
</html>
