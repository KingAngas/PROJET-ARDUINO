CREATE DATABASE IF NOT EXISTS maison CHARACTER SET utf8mb4;
USE maison;

CREATE TABLE meteo (
  id INT AUTO_INCREMENT PRIMARY KEY,
  temperature FLOAT NOT NULL,
  humidite FLOAT NOT NULL,
  luminosite INT NOT NULL,
  date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX (date)
);

CREATE TABLE badges (
  id INT AUTO_INCREMENT PRIMARY KEY,
  uid VARCHAR(32) NOT NULL UNIQUE,
  nom VARCHAR(100) NOT NULL,
  actif TINYINT(1) NOT NULL DEFAULT 1
);

CREATE TABLE acces (
  id INT AUTO_INCREMENT PRIMARY KEY,
  date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  uid VARCHAR(32) NULL,
  nom VARCHAR(100) NULL,
  autorise TINYINT(1) NOT NULL,
  cause ENUM('badge','site') NOT NULL DEFAULT 'badge',
  INDEX (date)
);

CREATE TABLE commandes (
  id INT PRIMARY KEY,
  mode_lampe ENUM('auto','on','off') NOT NULL DEFAULT 'auto',
  ouverture_porte TINYINT(1) NOT NULL DEFAULT 0
);

CREATE TABLE etat (
  id INT PRIMARY KEY,
  lampe_allumee TINYINT(1) NOT NULL DEFAULT 0,
  mode_actuel ENUM('auto','on','off') NOT NULL DEFAULT 'auto',
  porte_ouverte TINYINT(1) NOT NULL DEFAULT 0,
  derniere_connexion DATETIME NULL
);

CREATE TABLE alarmes (
  id INT AUTO_INCREMENT PRIMARY KEY,
  date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  type VARCHAR(50) NOT NULL,
  INDEX (date)
);

INSERT INTO commandes (id, mode_lampe, ouverture_porte) VALUES (1, 'auto', 0);
INSERT INTO etat (id) VALUES (1);
INSERT INTO badges (uid, nom, actif) VALUES ('35 4F 35 03', 'Badge principal', 1);