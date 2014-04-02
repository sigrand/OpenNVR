<?php if(!isset($code)) { ?>
<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('register', 'Password recovery'); ?></h3>
		</div>
		<div class="panel-body">
			<?php //TODO: save state edit form
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
				<label class="col-sm-4 control-label"><?php echo Yii::t('register', 'Enter your nick:'); ?></label>
				<div class="col-sm-8">
					<input type="text" name="nick" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-4 col-sm-8">
					<input type="submit" value="<?php echo Yii::t('register', 'Send recovery message'); ?>" class="btn btn-primary">
				</div>
			</div>
			<?php echo Yii::t('register', 'or'); ?><br/><br/>
			<div class="form-group">
				<label class="col-sm-4 control-label"><?php echo Yii::t('register', 'Enter your email:'); ?></label>
				<div class="col-sm-8">
					<input type="text" name="email" value="" class="form-control">
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-4 col-sm-8">
					<input type="submit" value="<?php echo Yii::t('register', 'Send recovery message'); ?>" class="btn btn-primary">
				</div>
			</div>
			<?php $this->endWidget(); ?><br/>
			<?php echo $result; ?>
		</div>
	</div>
</div>
<?php
} else {
	?>
	<div class="col-md-offset-3 col-md-5">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"></h3>
			</div>
			<div class="panel-body">
				<?php
				if($code == 1) { 
					echo Yii::t('register', 'There is no such user.');
				} elseif($code == 2) { 
					echo Yii::t('register', 'Wrong code.');
				} elseif($code == 3 || $code == 4 || $code == 5) { ?>
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
					<label class="col-sm-4 control-label"><?php echo Yii::t('register', 'Enter password:'); ?></label>
					<div class="col-sm-8">
						<input type="text" name="pass" value="" class="form-control">
					</div>
				</div>
				<div class="form-group">
					<label class="col-sm-4 control-label"><?php echo Yii::t('register', 'Enter password again:'); ?></label>
					<div class="col-sm-8">
						<input type="text" name="pass2" value="" class="form-control">
					</div>
				</div>
				<div class="form-group">
					<div class="col-sm-offset-4 col-sm-8">
						<input type="submit" value="<?php echo Yii::t('register', 'Change'); ?>" class="btn btn-primary">
					</div>
				</div>
				<?php $this->endWidget(); ?><br/>
				<?php  if($code == 4) {
					echo Yii::t('register', 'Passwords not equal!');
				} if($code == 5) {
					echo Yii::t('register', 'Password can\'t be empty.');
				} ?>

				<?php
			} elseif($code == 6) {
				echo Yii::t('register', 'Your password was successfully changed. go to {login} or {site}', array('{login}' => CHtml::link(Yii::t('register', 'login'), $this->createUrl('site/login')), '{site}' => CHtml::link(Yii::t('register', 'index'), $this->createUrl('site/index'))));
			}
			?>
		</div>
	</div>
</div>
<?php
}
?>
