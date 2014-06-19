<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'updates manager'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				$addLink = CHtml::link(Yii::t('admin', 'Make update'), $this->createUrl('admin/updateAdd'), array('class' => 'btn btn-success'));
				if(!empty($updates)) {
					echo $addLink;
					?>
					<table class="table table-striped">
						<thead>
							<th>#</th>
							<th><?php echo Yii::t('admin', 'File'); ?></th>
							<th><?php echo Yii::t('admin', 'Version'); ?></th>
							<th><?php echo Yii::t('admin', 'Time'); ?></th>
							<th><?php echo Yii::t('admin', 'Size'); ?></th>
							<th colspan="2"><?php echo Yii::t('admin', 'Actions'); ?></th>
						</thead>
						<tbody>
							<?php
							foreach($updates as $key => $update) {
								echo '<tr>
								<td>'.($key+1).'</td>
								<td>'.$update['file'].'</td>
								<td>'.$update['version'].'</td>
								<td>'.date('d-m-Y H:i:s', $update['time']).'</td>
								<td>'.$update['size'].'</td>
								<td>'.CHtml::link(Yii::t('admin', 'Delete'), $this->createUrl('admin/updateDelete', array('id' => $update['id'])), array('name' => 'del', 'class' => 'btn btn-danger')).'</td>
								<td>'.CHtml::link(Yii::t('admin', 'Download'), $this->createUrl('admin/updateDownload', array('id' => $update['id'])), array('class' => 'btn btn-warning', 'target' => '_blank')).'</td>
								</tr>';
							}
							?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', '<br/>List of updates  empty.<br/>');
				}
				echo $addLink;
				?>
			</div>
		</div>
	</div>