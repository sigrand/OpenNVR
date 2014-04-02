<?php
class BeginRequest extends CBehavior {
    public function attach($owner) {
        $owner->attachEventHandler('onBeginRequest', array($this, 'handleBeginRequest'));
        if(Yii::app()->urlManager->parseUrl(Yii::app()->request) == 'cams/check') {
            Yii::app()->detachEventHandler('onBeginRequest', array(Yii::app()->request, 'validateCsrfToken'));
        }
    }

    public function handleBeginRequest($event) {
        $app = Yii::app();
        $user = $app->user;
        if(isset($_POST['_lang'])) {
            $app->language = $_POST['_lang'];
            $app->user->setState('_lang', $_POST['_lang']);
            $cookie = new CHttpCookie('_lang', $_POST['_lang']);
            $cookie->expire = time() + (60*60*24*365); // (1 year)
            Yii::app()->request->cookies['_lang'] = $cookie;
        } elseif($app->user->hasState('_lang')) {
            $app->language = $app->user->getState('_lang');
        } elseif(isset(Yii::app()->request->cookies['_lang'])) {
            $app->language = Yii::app()->request->cookies['_lang']->value;
        }
    }
}