<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<?php echo Yii::t('errors', 'Error'),' ',$code; ?>
		</div>
		<div class="panel-body">
			<div class="error">
				<?php echo CHtml::encode($message); ?>
			</div>
		</div>
	</div>
</div>