<?php
/* @var $this ScreensController */
/* @var $data Screens */
?>

<div class="view">

	<b><?php echo CHtml::encode($data->getAttributeLabel('id')); ?>:</b>
	<?php echo CHtml::link(CHtml::encode($data->id), array('view', 'id'=>$data->id)); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('name')); ?>:</b>
	<?php echo CHtml::encode($data->name); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('owner_id')); ?>:</b>
	<?php echo CHtml::encode($data->owner_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam1_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam1_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam1_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam1_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam1_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam1_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam1_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam1_w); ?>
	<br />

	<?php /*
	<b><?php echo CHtml::encode($data->getAttributeLabel('cam1_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam1_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam1_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam1_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam2_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam2_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam2_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam2_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam2_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam2_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam2_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam2_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam2_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam2_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam2_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam2_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam3_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam3_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam3_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam3_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam3_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam3_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam3_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam3_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam3_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam3_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam3_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam3_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam4_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam4_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam4_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam4_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam4_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam4_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam4_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam4_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam4_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam4_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam4_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam4_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam5_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam5_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam5_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam5_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam5_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam5_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam5_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam5_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam5_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam5_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam5_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam5_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam6_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam6_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam6_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam6_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam6_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam6_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam6_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam6_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam6_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam6_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam6_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam6_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam7_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam7_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam7_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam7_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam7_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam7_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam7_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam7_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam7_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam7_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam7_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam7_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam8_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam8_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam8_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam8_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam8_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam8_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam8_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam8_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam8_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam8_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam8_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam8_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam9_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam9_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam9_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam9_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam9_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam9_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam9_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam9_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam9_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam9_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam9_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam9_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam10_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam10_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam10_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam10_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam10_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam10_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam10_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam10_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam10_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam10_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam10_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam10_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam11_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam11_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam11_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam11_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam11_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam11_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam11_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam11_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam11_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam11_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam11_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam11_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam12_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam12_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam12_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam12_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam12_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam12_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam12_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam12_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam12_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam12_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam12_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam12_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam13_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam13_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam13_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam13_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam13_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam13_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam13_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam13_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam13_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam13_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam13_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam13_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam14_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam14_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam14_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam14_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam14_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam14_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam14_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam14_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam14_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam14_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam14_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam14_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam15_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam15_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam15_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam15_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam15_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam15_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam15_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam15_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam15_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam15_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam15_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam15_descr); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam16_id')); ?>:</b>
	<?php echo CHtml::encode($data->cam16_id); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam16_x')); ?>:</b>
	<?php echo CHtml::encode($data->cam16_x); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam16_y')); ?>:</b>
	<?php echo CHtml::encode($data->cam16_y); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam16_w')); ?>:</b>
	<?php echo CHtml::encode($data->cam16_w); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam16_h')); ?>:</b>
	<?php echo CHtml::encode($data->cam16_h); ?>
	<br />

	<b><?php echo CHtml::encode($data->getAttributeLabel('cam16_descr')); ?>:</b>
	<?php echo CHtml::encode($data->cam16_descr); ?>
	<br />

	*/ ?>

</div>