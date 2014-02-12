<?php
echo CHtml::form('', 'post', array('class' => 'navbar-form navbar-inverse', 'data-style' => "btn-inverse"));
echo CHtml::dropDownList('_lang', $currentLang, $languages,
  array(
    'class' => 'form-control',
    'submit' => '',
    'csrf'=>true,
    )
  ); 
echo CHtml::endForm();
?>