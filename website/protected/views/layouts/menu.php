<li>
	<?php 
	$this->widget('application.components.widgets.LanguageSelector');
	?>
</li>
<?php if(!Yii::app()->user->isGuest) { ?>
	<?php if(Yii::app()->user->permissions == 2) { ?>
	<li class="dropdown">
		<a href="#" class="dropdown-toggle" data-toggle="dropdown"><?php echo Yii::t('menu', 'Manage'); ?> <b class="caret"></b></a>
		<ul class="dropdown-menu">
			<li><?php echo CHtml::link(Yii::t('menu', 'Users'), $this->createUrl('admin/users')); ?></li>
		</ul>
	</li>
	<?php } ?>
	<?php if(Yii::app()->user->isAdmin) { ?>
	<li class="dropdown">
		<a href="#" class="dropdown-toggle" data-toggle="dropdown"><?php echo Yii::t('menu', 'Admin'); ?> <b class="caret"></b></a>
		<ul class="dropdown-menu">
			<?php $this->renderPartial('//layouts/adminMenu'); ?>
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
	<li><?php echo CHtml::link(Yii::t('menu', 'Quotas'), $this->createUrl('quotas/manage')); ?></li>
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