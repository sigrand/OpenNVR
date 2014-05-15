<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Backups manager'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				$addLink = CHtml::link(Yii::t('admin', 'Make backup'), $this->createUrl('admin/backupAdd'), array('class' => 'btn btn-success'));
				if(!empty($backups)) {
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
							foreach($backups as $key => $backup) {
								echo '<tr>
								<td>'.($key+1).'</td>
								<td>'.$backup['file'].'</td>
								<td>'.$backup['version'].'</td>
								<td>'.date('d-m-Y H:i:s', $backup['time']).'</td>
								<td>'.$backup['size'].'</td>
								<td>'.CHtml::link(Yii::t('admin', 'Delete'), $this->createUrl('admin/backupDelete', array('id' => $backup['id'])), array('name' => 'del', 'class' => 'btn btn-danger')).'</td>
								<td>'.CHtml::link(Yii::t('admin', 'Download'), $this->createUrl('admin/backupDownload', array('id' => $backup['id'])), array('class' => 'btn btn-warning', 'target' => '_blank')).'</td>
								</tr>';
							}
							?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', '<br/>List of backups  empty.<br/>');
				}
				echo $addLink;
				?>
			</div>
		</div>
	</div>