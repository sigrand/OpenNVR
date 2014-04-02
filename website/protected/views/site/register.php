<?php
/* @var $this SiteController */
/* @var $model RegForm */
/* @var $form CActiveForm  */
Yii::app()->clientScript->registerCssFile(Yii::app()->baseUrl.'/css/passfield.min.css');
Yii::app()->clientScript->registerScriptFile(Yii::app()->baseUrl.'/js/passfield.min.js');
?>
<div class="col-md-offset-3 col-md-5">
    <div class="panel panel-default">
        <div class="panel-heading">
            <h3 class="panel-title"><?php echo Yii::t('register', 'Registration form'); ?></h3>
        </div>
        <div class="panel-body">
            <?php $form=$this->beginWidget('CActiveForm', array(
                'action' => $this->createUrl('register'),
                'id' => 'reg-form',
                'enableClientValidation' => true,
                'clientOptions' => array('validateOnSubmit' => true),
                'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
                )
                ); ?>
                <div class="form-group">
                    <?php echo $form->labelEx($model, 'nick', array('class' => 'col-sm-4 control-label')); ?>
                    <div class="col-sm-8">
                        <?php echo $form->textField($model, 'nick', array('class' => 'form-control')); ?>
                    </div>
                    <div class="col-sm-offset-4 col-sm-8">
                        <?php echo $form->error($model, 'nick'); ?>
                    </div>
                </div>
                <div class="form-group">
                    <?php echo $form->labelEx($model, 'email', array('class' => 'col-sm-4 control-label')); ?>
                    <div class="col-sm-8">
                        <?php echo $form->textField($model, 'email', array('class' => 'form-control')); ?>
                    </div>
                    <div class="col-sm-offset-4 col-sm-8">
                        <?php echo $form->error($model, 'email'); ?>
                    </div>
                </div>
                <div class="form-group">
                    <?php echo $form->labelEx($model, 'pass', array('class' => 'col-sm-4 control-label')); ?>
                    <div class="col-sm-8">
                        <?php echo $form->textField($model, 'pass', array('class' => 'form-control')); ?>
                    </div>
                    <div class="col-sm-offset-4 col-sm-8">
                        <?php echo $form->error($model, 'pass'); ?>
                    </div>
                </div>
                <div class="form-group">
                    <div class="col-sm-offset-2 col-sm-10">
                        <?php echo CHtml::submitButton(Yii::t('register', 'Register'), array('class' => 'btn btn-primary')); ?>
                    </div>
                </div>
                <?php $this->endWidget(); ?>
            </div>
        </div>
    </div>
    <script type="text/javascript">
    $("#RegForm_pass").passField({});
    </script>