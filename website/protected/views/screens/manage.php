<?php
/* @var $this CamsController */
?>
<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<?php if (Yii::app()->user->permissions == 3) { ?>
				<h3 class="panel-title"><?php echo Yii::t('screens', 'My screens'); ?></h3>
				<?php } else { ?>
				<h3 class="panel-title"><?php echo Yii::t('screens', 'All screens'); ?></h3>
				<?php } ?>
			</div>
			<div class="panel-body">
				<?php
				$addLink = CHtml::link('<img src="'.Yii::app()->request->baseUrl.'/images/add-icon.png" style="width:32px;"> '.Yii::t('screens', 'Add screen'), $this->createUrl('screens/create'));
				if(!empty($myscreens)) {
					echo $addLink;
					?>
					<table class="table table-striped">
						<thead>
							<th>#</th>
							<?php if (Yii::app()->user->permissions == 3) { ?>
							<th><?php echo Yii::t('screens', 'User ID'); ?></th>
							<?php } ?>
							<th><?php echo Yii::t('screens', 'Name'); ?></th>
							<th><?php echo Yii::t('screens', 'Edit'); ?></th>
							<th><?php echo Yii::t('screens', 'Delete'); ?></th>
						</thead>
						<tbody>
							<?php
/*							echo CHtml::beginForm($this->createUrl('screens/manage'), "post");*/
							foreach ($myscreens as $key => $screen) {
								echo '<tr>
								<td>'.($key+1).'</td>';
								if (Yii::app()->user->permissions == 3)
									echo '<td>'.$screen->owner_id.'</td>';
								echo '<td>'.CHtml::link(CHtml::encode($screen->name), $this->createUrl('screens/view/', array('id' => $screen->id)), array('target' => '_blank')).'</td>
								<td>'.CHtml::link(Yii::t('screens', 'Edit'), $this->createUrl('screens/update', array('id' => $screen->id))).'</td>
								<td>'.CHtml::link(Yii::t('screens', 'Delete'), $this->createUrl('screens/delete', array('id' => $screen->id))).'</td>
								</tr>';
							}
							?>
						</tbody>
					</table>
					<?php
				} else {
					?>
					<br/>
					<?php echo Yii::t('screens', 'My screens list is empty.'); ?>
					<br/>
					<?php
				}
				echo $addLink;
				?>
			</div>
		</div>
	</div>
	<script>
	$(document).ready(function(){
		$(".checkAll").click(function(){
			$('input[id*="CamsForm_cam_"]').not(this).prop('checked', this.checked);
		});
		$(".checkAllSh").click(function(){
			$('input[id*="CamsForm_shcam_"]').not(this).prop('checked', this.checked);
		});
		$(".checkAllP").click(function(){
			$('input[id*="CamsForm_pcam_"]').not(this).prop('checked', this.checked);
		});
	});
	</script>