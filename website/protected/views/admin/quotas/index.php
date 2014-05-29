<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Servers'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				$addLink = CHtml::link(Yii::t('admin', 'Add server'), $this->createUrl('admin/serverAdd'), array('class' => 'btn btn-success'));
				if(!empty($servers)) {
					echo $addLink;
					?>
					<table class="table table-striped">
						<thead>
							<th>#</th>
							<th><?php echo Yii::t('admin', 'Comment'); ?></th>
							<th><?php echo Yii::t('admin', 'Protocol'); ?></th>
							<th><?php echo Yii::t('admin', 'ip'); ?></th>
							<th><?php echo Yii::t('admin', 'port'); ?></th>
							<th><?php echo Yii::t('admin', 'web port'); ?></th>
							<th><?php echo Yii::t('admin', 'live port'); ?></th>
						</thead>
						<tbody>
							<?php
							foreach ($servers as $key => $serv) {
								echo '<tr>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($serv->comment).'</td>
								<td>'.$serv->protocol.'</td>
								<td>'.$serv->ip.'</td>
								<td>'.$serv->s_port.'</td>
								<td>'.$serv->w_port.'</td>
								<td>'.$serv->l_port.'</td>
								<td>'.CHtml::link(Yii::t('admin', 'Edit'), $this->createUrl('admin/serverEdit', array('id' => $serv->id)), array('name' => 'show', 'class' => 'btn btn-primary')).'</td>
								<td>'.CHtml::link(Yii::t('admin', 'Delete'), $this->createUrl('admin/serverDelete', array('id' => $serv->id)), array('name' => 'del', 'class' => 'btn btn-danger')).'</td>
								<td>'.CHtml::link(Yii::t('admin', 'Stat'), $this->createUrl('admin/stat', array('type'=> 'disk', 'id' => $serv->id)), array('class' => 'btn btn-warning')).'</td>
								</tr>';
							}
							?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', '<br/>List of servers empty.<br/>');
				}
				echo $addLink;
				?>
			</div>
		</div>
	</div>