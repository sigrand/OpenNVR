<?php
class LanguageSelector extends CWidget
{
    public function run()
    {
        $currentLang = Yii::app()->language;
        $languages = Yii::app()->params->languages;
        $this->render('languageSelector', array('currentLang' => $currentLang, 'languages'=>$languages));
    }
}