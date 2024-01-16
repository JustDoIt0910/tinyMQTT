USE tinymqtt_db;

DROP TABLE IF EXISTS `tinymqtt_user_table`;
DROP TABLE IF EXISTS `tinymqtt_retain_msgs_table`;
DROP TABLE IF EXISTS `tinymqtt_acl_table`;

CREATE TABLE `tinymqtt_user_table` (
    `id` INTEGER NOT NULL AUTO_INCREMENT,
    `username` VARCHAR(100) DEFAULT NULL,
    `password` VARCHAR(100) DEFAULT NULL,
    `created` DATETIME DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `mqtt_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `tinymqtt_retain_msgs_table` (
    `id` INTEGER NOT NULL AUTO_INCREMENT,
    `topic` VARCHAR(100) NOT NULL,
    `qos` TINYINT DEFAULT 1,
    `payload` MEDIUMTEXT NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `tinymqtt_acl_table` (
    `id` INTEGER NOT NULL AUTO_INCREMENT,
    `allow` TINYINT DEFAULT 1 COMMENT '0: deny, 1: allow',
    `ip` VARCHAR(60) DEFAULT '' COMMENT 'ip address',
    `username` VARCHAR(100) DEFAULT '' COMMENT 'username',
    `client_id` VARCHAR(100) DEFAULT '' COMMENT 'client id',
    `access` TINYINT NOT NULL COMMENT '0: subscribe, 1: publish, 2: pub_sub',
    `topic` VARCHAR(100) NOT NULL COMMENT 'topic filter',
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

