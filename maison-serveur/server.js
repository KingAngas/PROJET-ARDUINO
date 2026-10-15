require('dotenv').config();
const path = require('path');
const http = require('http');
const express = require('express');
const mysql = require('mysql2/promise');
const { Server } = require('socket.io');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.json());

app.use((req, res, next) => {
  console.log(new Date().toLocaleTimeString(), req.method, req.url);
  next();
});

const db = mysql.createPool({
  host: process.env.DB_HOST,
  user: process.env.DB_USER,
  password: process.env.DB_PASSWORD,
  database: process.env.DB_NAME,
  waitForConnections: true,
  connectionLimit: 5,
  timezone: 'Z',
  dateStrings: true
});

// Évite un try/catch dans chaque route
const wrap = (fn) => (req, res) =>
  fn(req, res).catch((err) => {
    console.error(err);
    res.status(500).json({ erreur: 'Erreur serveur' });
  });

const MODES = ['auto', 'on', 'off'];
const normUID = (s) => String(s || '').trim().toUpperCase();

// ---------- Lecture de l'état général ----------
async function getEtat() {
  const [[meteo]] = await db.query(
    'SELECT temperature, humidite, luminosite, date FROM meteo ORDER BY id DESC LIMIT 1'
  );
  const [[etat]] = await db.query(
    `SELECT lampe_allumee, mode_actuel, porte_ouverte, derniere_connexion,
            TIMESTAMPDIFF(SECOND, derniere_connexion, NOW()) AS secondes
     FROM etat WHERE id = 1`
  );
  const [[cmd]] = await db.query('SELECT mode_lampe FROM commandes WHERE id = 1');
  const [acces] = await db.query(
    'SELECT id, date, uid, nom, autorise, cause FROM acces ORDER BY id DESC LIMIT 10'
  );
  const [alarmes] = await db.query(
    'SELECT id, date, type FROM alarmes ORDER BY id DESC LIMIT 10'
  );

  return {
    meteo: meteo || null,
    lampe: {
      allumee: !!etat.lampe_allumee,
      modeActuel: etat.mode_actuel,
      modeDemande: cmd.mode_lampe
    },
    porte: { ouverte: !!etat.porte_ouverte },
    esp32: {
      enLigne: etat.secondes !== null && etat.secondes <= 30,
      derniereConnexion: etat.derniere_connexion
    },
    acces: acces.map((a) => ({ ...a, autorise: !!a.autorise })),
    alarmes
  };
}

async function diffuserEtat() {
  io.emit('etat', await getEtat());
}

// ---------- Mise à jour de "dernière connexion" à chaque appel de l'ESP32 ----------
const routesEsp32 = [
  '/api/meteo', '/api/acces', '/api/alarme',
  '/api/ordres', '/api/etat-reel', '/api/badges-actifs'
];
app.use(routesEsp32, (req, res, next) => {
  db.query('UPDATE etat SET derniere_connexion = NOW() WHERE id = 1').catch(console.error);
  next();
});

// =====================================================
//  ROUTES APPELÉES PAR L'ESP32
// =====================================================

// Mesure météo
app.post('/api/meteo', wrap(async (req, res) => {
  const { temperature, humidite, luminosite } = req.body;
  if ([temperature, humidite, luminosite].some((v) => typeof v !== 'number')) {
    return res.status(400).json({ erreur: 'Données invalides' });
  }
  await db.execute(
    'INSERT INTO meteo (temperature, humidite, luminosite) VALUES (?, ?, ?)',
    [temperature, humidite, luminosite]
  );
  res.json({ ok: true });
  diffuserEtat();
}));

// Passage de badge
app.post('/api/acces', wrap(async (req, res) => {
  const uid = normUID(req.body.uid);
  const autorise = req.body.autorise ? 1 : 0;
  const cause = req.body.cause === 'site' ? 'site' : 'badge';

  let nom = null;
  if (uid) {
    const [rows] = await db.execute('SELECT nom FROM badges WHERE uid = ?', [uid]);
    if (rows.length) nom = rows[0].nom;
  }
  await db.execute(
    'INSERT INTO acces (uid, nom, autorise, cause) VALUES (?, ?, ?, ?)',
    [uid || null, nom, autorise, cause]
  );
  res.json({ ok: true });
  diffuserEtat();
}));

// Alarme (3 refus d'affilée)
app.post('/api/alarme', wrap(async (req, res) => {
  const type = String(req.body.type || '3_refus').slice(0, 50);
  await db.execute('INSERT INTO alarmes (type) VALUES (?)', [type]);
  res.json({ ok: true });
  diffuserEtat();
}));

// L'ESP32 demande les ordres. L'ouverture de porte est "consommée" à la lecture.
app.get('/api/ordres', wrap(async (req, res) => {
  const conn = await db.getConnection();
  try {
    await conn.beginTransaction();
    const [[cmd]] = await conn.query(
      'SELECT mode_lampe, ouverture_porte FROM commandes WHERE id = 1 FOR UPDATE'
    );
    if (cmd.ouverture_porte) {
      await conn.query('UPDATE commandes SET ouverture_porte = 0 WHERE id = 1');
    }
    await conn.commit();
    res.json({ modeLampe: cmd.mode_lampe, ouvrirPorte: !!cmd.ouverture_porte });
  } catch (e) {
    await conn.rollback();
    throw e;
  } finally {
    conn.release();
  }
}));

// L'ESP32 envoie son état réel
app.post('/api/etat-reel', wrap(async (req, res) => {
  const { lampeAllumee, modeActuel, porteOuverte } = req.body;
  if (!MODES.includes(modeActuel)) {
    return res.status(400).json({ erreur: 'Mode invalide' });
  }
  await db.execute(
    'UPDATE etat SET lampe_allumee = ?, mode_actuel = ?, porte_ouverte = ? WHERE id = 1',
    [lampeAllumee ? 1 : 0, modeActuel, porteOuverte ? 1 : 0]
  );
  res.json({ ok: true });
  diffuserEtat();
}));

// Liste des badges actifs (téléchargée et gardée en mémoire par l'ESP32)
app.get('/api/badges-actifs', wrap(async (req, res) => {
  const [rows] = await db.query('SELECT uid FROM badges WHERE actif = 1');
  res.json({ badges: rows.map((r) => r.uid) });
}));

// =====================================================
//  ROUTES APPELÉES PAR LA PAGE VUE
// =====================================================

app.get('/api/etat', wrap(async (req, res) => {
  res.json(await getEtat());
}));

// Historique pour le graphique (par défaut les 6 dernières heures)
app.get('/api/meteo/historique', wrap(async (req, res) => {
  const heures = Math.min(Math.max(parseInt(req.query.heures, 10) || 6, 1), 168);
  const [rows] = await db.execute(
    `SELECT temperature, humidite, luminosite, date
     FROM meteo WHERE date >= NOW() - INTERVAL ? HOUR ORDER BY date ASC`,
    [heures]
  );
  res.json(rows);
}));

// Changer le mode de la lampe
app.post('/api/lampe', wrap(async (req, res) => {
  const { mode } = req.body;
  if (!MODES.includes(mode)) {
    return res.status(400).json({ erreur: 'Mode invalide (auto, on ou off)' });
  }
  await db.execute('UPDATE commandes SET mode_lampe = ? WHERE id = 1', [mode]);
  res.json({ ok: true });
  diffuserEtat();
}));

// Demander l'ouverture de la porte
app.post('/api/porte', wrap(async (req, res) => {
  await db.query('UPDATE commandes SET ouverture_porte = 1 WHERE id = 1');
  res.json({ ok: true });
}));

// ---------- Gestion des badges ----------
app.get('/api/badges', wrap(async (req, res) => {
  const [rows] = await db.query('SELECT id, uid, nom, actif FROM badges ORDER BY nom');
  res.json(rows.map((b) => ({ ...b, actif: !!b.actif })));
}));

app.post('/api/badges', wrap(async (req, res) => {
  const uid = normUID(req.body.uid);
  const nom = String(req.body.nom || '').trim().slice(0, 100);
  if (!uid || !nom) return res.status(400).json({ erreur: 'UID et nom obligatoires' });
  try {
    const [r] = await db.execute('INSERT INTO badges (uid, nom) VALUES (?, ?)', [uid, nom]);
    res.status(201).json({ id: r.insertId, uid, nom, actif: true });
    io.emit('badges');
  } catch (e) {
    if (e.code === 'ER_DUP_ENTRY') return res.status(409).json({ erreur: 'UID déjà enregistré' });
    throw e;
  }
}));

// Renommer et/ou activer/désactiver
app.patch('/api/badges/:id', wrap(async (req, res) => {
  const id = parseInt(req.params.id, 10);
  const champs = [];
  const valeurs = [];

  if (typeof req.body.nom === 'string' && req.body.nom.trim()) {
    champs.push('nom = ?');
    valeurs.push(req.body.nom.trim().slice(0, 100));
  }
  if (typeof req.body.actif === 'boolean') {
    champs.push('actif = ?');
    valeurs.push(req.body.actif ? 1 : 0);
  }
  if (!champs.length) return res.status(400).json({ erreur: 'Rien à modifier' });

  valeurs.push(id);
  const [r] = await db.execute(`UPDATE badges SET ${champs.join(', ')} WHERE id = ?`, valeurs);
  if (!r.affectedRows) return res.status(404).json({ erreur: 'Badge introuvable' });
  res.json({ ok: true });
  io.emit('badges');
}));

app.delete('/api/badges/:id', wrap(async (req, res) => {
  const [r] = await db.execute('DELETE FROM badges WHERE id = ?', [parseInt(req.params.id, 10)]);
  if (!r.affectedRows) return res.status(404).json({ erreur: 'Badge introuvable' });
  res.json({ ok: true });
  io.emit('badges');
}));

// =====================================================
//  Temps réel + page Vue compilée
// =====================================================
io.on('connection', async (socket) => {
  try {
    socket.emit('etat', await getEtat());
  } catch (e) {
    console.error(e);
  }
});

// Rafraîchit l'état toutes les 5 s pour que "ESP32 hors ligne" apparaisse tout seul
setInterval(() => diffuserEtat().catch(console.error), 5000);

// Sert la page Vue compilée (copiez le dossier "dist" dans "public")
app.use(express.static(path.join(__dirname, 'public')));

const PORT = process.env.PORT || 3000;
server.listen(PORT, '0.0.0.0', () => {
  console.log(`Serveur démarré sur le port ${PORT}`);
});