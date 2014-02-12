<?php
/* @var $this SiteController */
/* @var $model ContactForm */
/* @var $form CActiveForm */

?>
<?php if(!isset($activation)) {
	echo Yii::t('register', '
		Регистрация прошла успешно.<br/>
		Для авторизации, вам необходимо подтвердить свой email.<br/>
		Письмо с кодом подтверждения отправлено вам на почту.<br/>');
	echo CHtml::link(Yii::t('register', 'Перейти на сайт'), Yii::app()->createUrl('site/index'));
} elseif($activation == 1) { 
	echo Yii::t('register', 'Данный пользователь, уже подтвердил свой email.');
} elseif($activation == 2) { 
	echo Yii::t('register', 'Такого пользователя нет.');
} elseif($activation == 3) { 
	echo Yii::t('register', 'Неправильный код.');
} elseif($activation == 4) { 
	echo Yii::t('register', 'Вы успешно подтвердили свой email. Теперь вы можете авторизоваться.');
} ?>
