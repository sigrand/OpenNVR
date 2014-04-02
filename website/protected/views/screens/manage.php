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
				$addLink = CHtml::link(Yii::t('screens', 'Add screen'), $this->createUrl('screens/create'), array('class' => 'btn btn-success'));
				if(!empty($myscreens)) {
					echo $addLink;
					?>
					<table class="table table-striped">
						<thead>
							<th>#</th>
							<?php if (Yii::app()->user->permissions == 3) { ?>
							<th><?php echo Yii::t('screens', 'User'); ?></th>
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
								if (Yii::app()->user->permissions == 3) {
									echo '<td>'.$screen->owner->nick.'</td>';
								}
								echo '<td>'.CHtml::link(CHtml::encode($screen->name), $this->createUrl('screens/view/', array('id' => $screen->id)), array('target' => '_blank')).'</td>
								<td>'.CHtml::link(Yii::t('screens', 'Edit'), $this->createUrl('screens/update', array('id' => $screen->id)), array('class' => 'btn btn-primary')).'</td>
								<td>'.CHtml::link(Yii::t('screens', 'Delete'), $this->createUrl('screens/delete', array('id' => $screen->id)), array('class' => 'btn btn-danger')).'</td>
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