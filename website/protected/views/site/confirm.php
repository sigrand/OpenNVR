<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
		</div>
		<div class="panel-body">
			<?php
			if(!isset($activation)) {
				if(Settings::model()->getValue('mail_confirm')) {
					echo Yii::t('register', 'Registration was successful.<br/>To use your account, you need to confirm your email address.<br/>Confirmation code sent to your email.<br/>');
				} else {
					echo Yii::t('register', 'Registration was successful.<br/>');
				}
				echo CHtml::link(Yii::t('register', 'Go to site'), Yii::app()->createUrl('site/index'));
			} elseif($activation == 1) {
				echo Yii::t('register', 'This user already confirmed his email address.');
			} elseif($activation == 2) {
				echo Yii::t('register', 'There is no such user.');
			} elseif($activation == 3) {
				echo Yii::t('register', 'Wrong code.');
			} elseif($activation == 4) {
				echo Yii::t('register', 'You successfully confirmed your email address. Now you can login.');
			}
			?>
		</div>
	</div>
</div>