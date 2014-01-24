<?php
/* @var $this SiteController */
/* @var $model ContactForm */
/* @var $form CActiveForm */

?>
<?php if(!isset($activation)) { ?>
Регистрация прошла успешно.<br/>
Для авторизации, вам необходимо подтвердить свой email.<br/>
Письмо с кодом подтверждения отправлено вам на почту.<br/>
<?php echo CHtml::link('Перейти на сайт', Yii::app()->createUrl('site/index')); ?>
<?php } elseif($activation == 1) { ?>
Данный пользователь, уже подтвердил свой email.
<?php } elseif($activation == 2) { ?>
Такого пользователя нет.
<?php } elseif($activation == 3) { ?>
Неправильный код.
<?php } elseif($activation == 4) { ?>
Вы успешно подтвердили свой email. Теперь вы можете авторизоваться.
<?php } ?>
