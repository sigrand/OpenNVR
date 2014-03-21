<?php
/* @var $this ScreensController */
/* @var $model Screens */

/*
$this->breadcrumbs=array(
	'Screens'=>array('index'),
	'Create',
);

$this->menu=array(
	array('label'=>'List Screens', 'url'=>array('index')),
	array('label'=>'Manage Screens', 'url'=>array('admin')),
);
*/
?>

<!-- <h1>Create Screens</h1> -->


		<div class="panel panel-default" style="margin-left:5px;margin-right:5px;">
			<div class="panel-heading">
				<h3 class="panel-title">
					<?php echo Yii::t('screens', 'New screen'); ?>
				</h3>
			</div>
			<div class="panel-body">
				<input type='button' class='btn btn-primary' value='1' onClick="window.location.href+='/type/1';"></input>
				<input type='button' class='btn btn-primary' value='2x2' onClick="window.location.href+='/type/2x2';"></input>
				<input type='button' class='btn btn-primary' value='3x3' onClick="window.location.href+='/type/3x3';"></input>
				<input type='button' class='btn btn-primary' value='4x4' onClick="window.location.href+='/type/4x4';"></input>
				<input type='button' class='btn btn-primary' value="<?php echo Yii::t('screens', 'Custom'); ?>" onClick="window.location.href+='/type/custom';"></input>
			</div>
		</div>
