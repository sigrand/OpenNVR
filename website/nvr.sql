-- Adminer 3.7.1 MySQL dump

SET NAMES utf8;
SET foreign_key_checks = 0;
SET time_zone = '+07:00';
SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO';

DROP TABLE IF EXISTS `cams`;
CREATE TABLE `cams` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `user_id` int(10) NOT NULL,
  `name` tinytext NOT NULL,
  `desc` text,
  `url` varchar(2000) NOT NULL,
  `prev_url` varchar(2000) DEFAULT NULL,
  `show` tinyint(1) DEFAULT '0',
  `public` tinyint(1) DEFAULT '0',
  `time_offset` varchar(4) NOT NULL DEFAULT '+0',
  `coordinates` varchar(100) DEFAULT '',
  `record` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

DROP TABLE IF EXISTS `notifications`;
CREATE TABLE `notifications` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `msg` text NOT NULL,
  `creator_id` int(10) NOT NULL DEFAULT '0',
  `dest_id` int(10) NOT NULL DEFAULT '0',
  `is_new` tinyint(4) NOT NULL DEFAULT '1',
  `time` int(10) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

DROP TABLE IF EXISTS `shared`;
CREATE TABLE `shared` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `owner_id` int(10) NOT NULL,
  `user_id` int(10) NOT NULL,
  `cam_id` int(10) NOT NULL,
  `show` tinyint(4) NOT NULL DEFAULT '0',
  `public` tinyint(4) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

DROP TABLE IF EXISTS `users`;
CREATE TABLE `users` (
  `id` int(7) NOT NULL AUTO_INCREMENT,
  `nick` varchar(150) DEFAULT NULL,
  `email` varchar(250) DEFAULT NULL,
  `pass` varchar(100) DEFAULT NULL,
  `status` tinyint(4) DEFAULT NULL,
  `salt` varchar(32) DEFAULT NULL,
  `time_offset` varchar(4) NOT NULL DEFAULT '+0',
  `options` tinyint(4) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

INSERT INTO `users` (`id`, `nick`, `email`, `pass`, `status`, `salt`, `time_offset`, `options`) VALUES
(0,	'admin',	'admin@admin.admin',	'$1$99pVDpkV$3PrWcVWefq9JopB8zsHMR0',	3,	'3f9d03118755b0406e61376cb7a28d95',	'+0',	0);

DROP TABLE IF EXISTS `screens`;
CREATE TABLE `screens` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `name` varchar(150) DEFAULT NULL,
  `owner_id` int(10) NOT NULL,
  `cam1_id` int(10) NOT NULL,
  `cam1_x` int(10) NOT NULL,
  `cam1_y` int(10) NOT NULL,
  `cam1_w` int(10) NOT NULL,
  `cam1_h` int(10) NOT NULL,
  `cam1_descr` varchar(150) DEFAULT NULL,
  `cam2_id` int(10) NOT NULL,
  `cam2_x` int(10) NOT NULL,
  `cam2_y` int(10) NOT NULL,
  `cam2_w` int(10) NOT NULL,
  `cam2_h` int(10) NOT NULL,
  `cam2_descr` varchar(150) DEFAULT NULL,
  `cam3_id` int(10) NOT NULL,
  `cam3_x` int(10) NOT NULL,
  `cam3_y` int(10) NOT NULL,
  `cam3_w` int(10) NOT NULL,
  `cam3_h` int(10) NOT NULL,
  `cam3_descr` varchar(150) DEFAULT NULL,
  `cam4_id` int(10) NOT NULL,
  `cam4_x` int(10) NOT NULL,
  `cam4_y` int(10) NOT NULL,
  `cam4_w` int(10) NOT NULL,
  `cam4_h` int(10) NOT NULL,
  `cam4_descr` varchar(150) DEFAULT NULL,
  `cam5_id` int(10) NOT NULL,
  `cam5_x` int(10) NOT NULL,
  `cam5_y` int(10) NOT NULL,
  `cam5_w` int(10) NOT NULL,
  `cam5_h` int(10) NOT NULL,
  `cam5_descr` varchar(150) DEFAULT NULL,
  `cam6_id` int(10) NOT NULL,
  `cam6_x` int(10) NOT NULL,
  `cam6_y` int(10) NOT NULL,
  `cam6_w` int(10) NOT NULL,
  `cam6_h` int(10) NOT NULL,
  `cam6_descr` varchar(150) DEFAULT NULL,
  `cam7_id` int(10) NOT NULL,
  `cam7_x` int(10) NOT NULL,
  `cam7_y` int(10) NOT NULL,
  `cam7_w` int(10) NOT NULL,
  `cam7_h` int(10) NOT NULL,
  `cam7_descr` varchar(150) DEFAULT NULL,
  `cam8_id` int(10) NOT NULL,
  `cam8_x` int(10) NOT NULL,
  `cam8_y` int(10) NOT NULL,
  `cam8_w` int(10) NOT NULL,
  `cam8_h` int(10) NOT NULL,
  `cam8_descr` varchar(150) DEFAULT NULL,
  `cam9_id` int(10) NOT NULL,
  `cam9_x` int(10) NOT NULL,
  `cam9_y` int(10) NOT NULL,
  `cam9_w` int(10) NOT NULL,
  `cam9_h` int(10) NOT NULL,
  `cam9_descr` varchar(150) DEFAULT NULL,
  `cam10_id` int(10) NOT NULL,
  `cam10_x` int(10) NOT NULL,
  `cam10_y` int(10) NOT NULL,
  `cam10_w` int(10) NOT NULL,
  `cam10_h` int(10) NOT NULL,
  `cam10_descr` varchar(150) DEFAULT NULL,
  `cam11_id` int(10) NOT NULL,
  `cam11_x` int(10) NOT NULL,
  `cam11_y` int(10) NOT NULL,
  `cam11_w` int(10) NOT NULL,
  `cam11_h` int(10) NOT NULL,
  `cam11_descr` varchar(150) DEFAULT NULL,
  `cam12_id` int(10) NOT NULL,
  `cam12_x` int(10) NOT NULL,
  `cam12_y` int(10) NOT NULL,
  `cam12_w` int(10) NOT NULL,
  `cam12_h` int(10) NOT NULL,
  `cam12_descr` varchar(150) DEFAULT NULL,
  `cam13_id` int(10) NOT NULL,
  `cam13_x` int(10) NOT NULL,
  `cam13_y` int(10) NOT NULL,
  `cam13_w` int(10) NOT NULL,
  `cam13_h` int(10) NOT NULL,
  `cam13_descr` varchar(150) DEFAULT NULL,
  `cam14_id` int(10) NOT NULL,
  `cam14_x` int(10) NOT NULL,
  `cam14_y` int(10) NOT NULL,
  `cam14_w` int(10) NOT NULL,
  `cam14_h` int(10) NOT NULL,
  `cam14_descr` varchar(150) DEFAULT NULL,
  `cam15_id` int(10) NOT NULL,
  `cam15_x` int(10) NOT NULL,
  `cam15_y` int(10) NOT NULL,
  `cam15_w` int(10) NOT NULL,
  `cam15_h` int(10) NOT NULL,
  `cam15_descr` varchar(150) DEFAULT NULL,
  `cam16_id` int(10) NOT NULL,
  `cam16_x` int(10) NOT NULL,
  `cam16_y` int(10) NOT NULL,
  `cam16_w` int(10) NOT NULL,
  `cam16_h` int(10) NOT NULL,
  `cam16_descr` varchar(150) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 2014-01-27 18:01:23
