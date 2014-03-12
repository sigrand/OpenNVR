<?php
/* @var $this SiteController */
/* @var $model ContactForm */
/* @var $form CActiveForm */

?>
<?php if(!isset($activation)) {
	echo Yii::t('register', '
		Registration was successful.<br/>
		To use your account, you need to confirm your email address.<br/>
		Confirmation code sent to your email.<br/>');
	echo CHtml::link(Yii::t('register', 'Go to site'), Yii::app()->createUrl('site/index'));
} elseif($activation == 1) {
	echo Yii::t('register', 'This user already confirmed his email address.');
} elseif($activation == 2) {
	echo Yii::t('register', 'There is no such user.');
} elseif($activation == 3) {
	echo Yii::t('register', 'Wrong code.');
} elseif($activation == 4) {
	echo Yii::t('register', 'You successfully confirmed your email address. Now you can login.');
} ?>
