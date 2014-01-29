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
(0,	'admin',	'admin@admin.admin',	'$1$99pVDpkV$3PrWcVWefq9JopB8zsHMR0',	3,	'3f9d03118755b0406e61376cb7a28d95',	'+0',	0)

-- 2014-01-27 18:01:23
