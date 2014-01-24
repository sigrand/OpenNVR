<?php
/* @var $this SiteController */
?>
<?php if(!isset($code)) { ?>
<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title">Введите email и пароль</h3>
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
				<label class="col-sm-4 control-label">Введите Ник:</label>
				<div class="col-sm-8">
					<input type="text" name="nick" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-4 col-sm-8">
					<input type="submit" value="Восстановить" class="btn btn-primary">
				</div>
			</div>
			или<br/><br/>
			<div class="form-group">
				<label class="col-sm-4 control-label">Введите email:</label>
				<div class="col-sm-8">
					<input type="text" name="email" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-4 col-sm-8">
					<input type="submit" value="Восстановить" class="btn btn-primary">
				</div>
			</div>
			<?php $this->endWidget(); ?><br/>
			<?php echo $result; ?>
		</div>
	</div>
</div>
<?php } elseif($code == 1) { ?>
Такого пользователя нет.
<?php } elseif($code == 2) { ?>
Неправильный код.
<?php } elseif($code == 3 || $code == 4 || $code == 5) { ?>
<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title">Изменение пароля</h3>
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
				<label class="col-sm-4 control-label">Введите пароль:</label>
				<div class="col-sm-8">
					<input type="text" name="pass" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<label class="col-sm-4 control-label">Подтвердите пароль:</label>
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
			<?php  if($code == 4) { ?>
			Пароли не совпадают!
			<?php } if($code == 5) { ?>
			Пароль не может быть пустым.
			<?php } ?>
		</div>
	</div>
</div>
<?php } elseif($code == 6) { ?>
Ваш пароль успешно изменен.
<?php } ?>
