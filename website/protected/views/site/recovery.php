<?php
/* @var $this SiteController */
?>
<?php if(!isset($code)) { ?>
<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('register', 'Введите email и пароль'); ?></h3>
		</div>
		<div class="panel-body">
			<?php
			$form = $this->beginWidget('CActiveForm', array(
				'action' => $this->createUrl('site/recovery'),
				'id' => 'login-form',
				'enableClientValidation' => false,
				'clientOptions' => array(
					'validateOnSubmit' => true,
					),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
			);
			?>
			<div class="form-group">
				<label class="col-sm-4 control-label"><?php echo Yii::t('register', 'Введите Ник:'); ?></label>
				<div class="col-sm-8">
					<input type="text" name="nick" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-4 col-sm-8">
					<input type="submit" value="<?php echo Yii::t('register', 'Восстановить'); ?>" class="btn btn-primary">
				</div>
			</div>
			<?php echo Yii::t('register', 'или'); ?><br/><br/>
			<div class="form-group">
				<label class="col-sm-4 control-label"><?php echo Yii::t('register', 'Введите email:'); ?></label>
				<div class="col-sm-8">
					<input type="text" name="email" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-4 col-sm-8">
					<input type="submit" value="<?php echo Yii::t('register', 'Восстановить'); ?>" class="btn btn-primary">
				</div>
			</div>
			<?php $this->endWidget(); ?><br/>
			<?php echo $result; ?>
		</div>
	</div>
</div>
<?php } elseif($code == 1) { 
	echo Yii::t('register', 'Такого пользователя нет.');
} elseif($code == 2) { 
	echo Yii::t('register', 'Неправильный код.');
} elseif($code == 3 || $code == 4 || $code == 5) { ?>
<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('register', 'Изменение пароля'); ?></h3>
		</div>
		<div class="panel-body">
			<?php
			$form = $this->beginWidget('CActiveForm', array(
				'action' => $this->createUrl('site/recovery', array('user' => $_GET['user'], 'code' => $_GET['code'])),
				'id' => 'login-form',
				'enableClientValidation' => false,
				'clientOptions' => array(
					'validateOnSubmit' => true,
					),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
			);
			?>
			<div class="form-group">
				<label class="col-sm-4 control-label"><?php echo Yii::t('register', 'Введите пароль:'); ?></label>
				<div class="col-sm-8">
					<input type="text" name="pass" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<label class="col-sm-4 control-label"><?php echo Yii::t('register', 'Подтвердите пароль:'); ?></label>
				<div class="col-sm-8">
					<input type="text" name="pass2" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-4 col-sm-8">
					<input type="submit" value="Изменить" class="btn btn-primary">
				</div>
			</div>
			<?php $this->endWidget(); ?><br/>
			<?php  if($code == 4) {
				echo Yii::t('register', 'Пароли не совпадают!');
			} if($code == 5) {
				echo Yii::t('register', 'Пароль не может быть пустым.');
			} ?>
		</div>
	</div>
</div>
<?php
} elseif($code == 6) {
	echo Yii::t('register', 'Ваш пароль успешно изменен.');
}
?>
