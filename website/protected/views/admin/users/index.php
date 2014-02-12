<?php
/* @var $this AdminController */
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
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Админы'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($admins)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllA", array ("class" => "checkAllA")); ?></th>
							<th>#</th>
							<th>Ник</th>
							<th>Почта</th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($admins as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'auser_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="2"></td>
								<td><?php echo CHtml::submitButton(Yii::t('admin', 'Понизить'), array('name' => 'dismiss', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('admin', 'Забанить'), array('name' => 'ban', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', 'Список админов пуст.<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Забаненные'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($operators)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllZ", array ("class" => "checkAllZ")); ?></th>
							<th>#</th>
							<th>Ник</th>
							<th>Почта</th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($operators as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'zuser_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="1"></td>
								<td><?php echo CHtml::submitButton('Повысить', array('name' => 'levelup', 'class' => 'btn btn-success')); ?></td>
								<td><?php echo CHtml::submitButton('Понизить', array('name' => 'dismiss', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton('Забанить', array('name' => 'ban', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					?>
					Список операторов пуст.
					<br/>
					<?php
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title">Наблюдатели</h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($viewers)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllZ", array ("class" => "checkAllZ")); ?></th>
							<th>#</th>
							<th>Ник</th>
							<th>Почта</th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($viewers as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'zuser_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="2"></td>
								<td><?php echo CHtml::submitButton('Повысить', array('name' => 'levelup', 'class' => 'btn btn-success')); ?></td>
								<td><?php echo CHtml::submitButton('Забанить', array('name' => 'ban', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					?>
					Список наблюдателей пуст.
					<br/>
					<?php
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title">Забаненные</h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($banned)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllZ", array ("class" => "checkAllZ")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('admin', 'Ник'); ?></th>
							<th><?php echo Yii::t('admin', 'Почта'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($banned as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'zuser_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="2"></td>
								<td><?php echo CHtml::submitButton(Yii::t('admin', 'Повысить'), array('name' => 'levelup', 'class' => 'btn btn-success')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('admin', 'разбанить'), array('name' => 'unban', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton('Разбанить', array('name' => 'unban', 'class' => 'btn btn-danger')); ?></td>

							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', 'Список забаненных пуст.<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Все остальные'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($all)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAll", array ("class" => "checkAll")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('admin', 'Ник'); ?></th>
							<th><?php echo Yii::t('admin', 'Почта'); ?></th>
							<th><?php echo Yii::t('admin', 'Статус'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($all as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'user_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								<td>'.($user->status == 1 ? Yii::t('admin', 'Активный') : Yii::t('admin', 'Неактивированный')).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="2">
									<?php
									$this->widget('CLinkPager', array(
										'pages' => $pages,
										'header' => Yii::t('admin', 'Перейти на страницу: '),
										'nextPageLabel' => Yii::t('admin', 'далее'),
										'prevPageLabel' => Yii::t('admin', 'назад'),
										));
										?>
									</td>
									<td><?php echo CHtml::submitButton(Yii::t('admin', 'Активировать'), array('name' => 'active', 'class' => 'btn btn-primary')); ?></td>
									<td><?php echo CHtml::submitButton(Yii::t('admin', 'Повысить'), array('name' => 'levelup', 'class' => 'btn btn-success')); ?></td>
									<td><?php echo CHtml::submitButton(Yii::t('admin', 'Забанить'), array('name' => 'ban', 'class' => 'btn btn-danger')); ?></td>
								</tr>
								<?php echo CHtml::endForm(); ?>
							</tbody>
						</table>
						<?php
					} else {
						echo Yii::t('admin', 'Список пользователей пуст.<br/>');
					}
					?>
				</div>
			</div>
		</div>
		<script>
		$(document).ready(function(){
			$(".checkAll").click(function(){
				$('input[id*="UsersForm_user_"]').not(this).prop('checked', this.checked);
			});
			$(".checkAllA").click(function(){
				$('input[id*="UsersForm_auser_"]').not(this).prop('checked', this.checked);
			});
			$(".checkAllZ").click(function(){
				$('input[id*="UsersForm_zuser_"]').not(this).prop('checked', this.checked);
			});
		});
		</script>