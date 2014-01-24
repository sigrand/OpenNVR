<?php
/* @var $this CamsController */

if (!Yii::app()->user->isGuest) {
    ?>
    <div class="col-sm-12">
        <div class="panel panel-default">
            <div class="panel-heading">
                <h3 class="panel-title">Мои камеры</h3>
            </div>
            <div class="panel-body">
                <ul class="list-group">
                    <li class="list-group-item">
                        <div class="row">
                            <?php if (!empty($myCams)) {
                            foreach ($myCams as $key => $cam) {
                            ?>
                            <div class="col-sm-6">
                                <iframe
                                    id="cam_<?php echo $key; ?>"
                                    style="width:100%; display: none;"
                                    class="img-thumbnail"
                                    src="<?php echo $this->createUrl('cams/fullscreen', array('id' => $cam->id, 'preview' => 1)); ?>">
                                    </iframe>
                                <br/>
                                <?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
                                <?php echo $cam->desc; ?><br/>
                            </div>
                            <?php
                            if ($key % 2) {
                            ?>
                        </div>
                    </li>
                    <li class="list-group-item">
                        <div class="row">
                            <?php
                            }
                            }
                            ?>
                            <?php
                            } else {
                                ?>
                                Камер для отображения нет.
                            <?php } ?>
                </ul>
            </div>
        </div>
    </div>
    <script type="text/javascript">
        var cam = $("#cam_0");
        var h = cam.innerWidth() / 1.3;
        $('iframe[id*="cam_"]').height(h).show();
        cam.height(h).show();
    </script>
    <div class="col-sm-12">
        <div class="panel panel-default">
            <div class="panel-heading">
                <h3 class="panel-title">Расшаренные для меня камеры</h3>
            </div>
            <div class="panel-body">
                <ul class="list-group">
                    <li class="list-group-item">
                        <div class="row">
                            <?php if (!empty($mySharedCams)) {
                            foreach ($mySharedCams as $key => $cam) {
                            if (isset($cam->cam_id)) {
                                $cam = $cam->cam;
                            }
                            ?>
                            <div class="col-sm-6">
                                <iframe
                                    id="scam_<?php echo $key; ?>"
                                    style="width:100%; display: none;"
                                    class="img-thumbnail"
                                    src="<?php echo $this->createUrl('cams/fullscreen', array('id' => $cam->id, 'preview' => 1)); ?>">
                                </iframe>
                                <br/>
                                <?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
                                <?php echo $cam->desc; ?><br/>
                            </div>
                            <?php
                            if ($key % 2) {
                            ?>
                        </div>
                    </li>
                    <li class="list-group-item">
                        <div class="row">
                            <?php
                            }
                            }
                            ?>
                            <script type="text/javascript">
                                var cam = $("#scam_0");
                                var h = cam.innerWidth() / 1.3;
                                $('iframe[id*="scam_"]').height(h).show();
                                cam.height(h).show();
                            </script>
                        </div>
                    </li>
                    <?php
                    } else {
                        ?>
                        Камер для отображения нет.
                    <?php } ?>
                </ul>
            </div>
        </div>
    </div>
<?php
}
?>
<div class="col-sm-12">
    <div class="panel panel-default">
        <div class="panel-heading">
            <h3 class="panel-title">Публичные камеры</h3>
        </div>
        <div class="panel-body">
            <ul class="list-group">
                <li class="list-group-item">
                    <div class="row">
                        <?php if (!empty($myPublicCams)) {
                        foreach ($myPublicCams as $key => $cam) {
                        if (isset($cam->cam_id)) {
                            $cam = $cam->cam;
                        }
                        ?>
                        <div class="col-sm-6">
                            <iframe
                                id="pcam_<?php echo $key; ?>"
                                style="width:100%; display: none;"
                                class="img-thumbnail"
                                src="<?php echo $this->createUrl('cams/fullscreen', array('id' => $cam->id, 'preview' => 1)); ?>">
                            </iframe>
                            <br/>
                            <?php echo CHtml::link($cam->name, $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')); ?>
                            <?php echo $cam->desc; ?><br/>
                        </div>
                        <?php
                        if ($key % 2) {
                        ?>
                    </div>
                </li>
                <li class="list-group-item">
                    <div class="row">
                        <?php
                        }
                        }
                        ?>
                        <script type="text/javascript">
                            var cam = $("#pcam_0");
                            var h = cam.innerWidth() / 1.3;
                            $('iframe[id*="pcam_"]').height(h).show();
                            cam.height(h).show();
                        </script>
                    </div>
                </li>
                <?php
                } else {
                    ?>
                    Камер для отображения нет.
                <?php } ?>
            </ul>
        </div>
    </div>
</div>

