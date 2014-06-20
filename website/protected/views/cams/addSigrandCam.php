<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('cams', 'Add Sigrand cam'); ?></h3>
		</div>
		<div class="panel-body">
			<?php echo CHtml::beginForm(); ?>
					<div class="form-group">
						<div class="col-sm-8 control-label"><b><?php echo Yii::t('cams', 'MAC address'); ?></b></div>
						<div class="col-sm-8">
							<?php echo CHtml::textField('mac', '', array('class' => 'form-control')); ?>
						</div>
					</div>
				<div class="form-group">
					<div class="col-sm-offset-2 col-sm-10">
						<?php echo CHtml::submitButton(Yii::t('cams', 'Add'), array('class' => 'btn btn-primary')); ?>
					</div>
				</div>
				<?php echo CHtml::endForm(); ?>
			</div>
		</div>
	</div>
</div>
