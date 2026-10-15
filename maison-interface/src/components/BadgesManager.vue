<script setup>
import { ref, computed, watch, onMounted } from 'vue'
import { get, post, patch, del } from '../api'
import { etat, badgesVersion } from '../store'

const badges = ref([])
const uid = ref('')
const nom = ref('')
const message = ref('')
const erreur = ref('')

async function charger() {
  try {
    badges.value = await get('/api/badges')
  } catch (e) {
    erreur.value = e.message
  }
}
onMounted(charger)
watch(badgesVersion, charger)   // le serveur prévient quand les badges changent

// Dernier badge refusé qui n'est pas déjà enregistré
const dernierRefuse = computed(() =>
  (etat.value?.acces || []).find(
    (a) => !a.autorise && a.uid && !badges.value.some((b) => b.uid === a.uid)
  )
)

function reprendreRefuse() {
  if (!dernierRefuse.value) return
  uid.value = dernierRefuse.value.uid
  message.value = 'UID repris : donnez un nom puis cliquez sur Ajouter.'
  erreur.value = ''
}

async function agir(fn, ok) {
  erreur.value = ''
  message.value = ''
  try {
    await fn()
    if (ok) message.value = ok
  } catch (e) {
    erreur.value = e.message
  }
}

const ajouter = () =>
  agir(async () => {
    await post('/api/badges', { uid: uid.value, nom: nom.value })
    uid.value = ''
    nom.value = ''
  }, 'Badge ajouté.')

const basculer = (b) => agir(() => patch(`/api/badges/${b.id}`, { actif: !b.actif }))

function renommer(b) {
  const n = prompt('Nouveau nom :', b.nom)
  if (n && n.trim()) agir(() => patch(`/api/badges/${b.id}`, { nom: n.trim() }))
}

function supprimer(b) {
  if (confirm(`Supprimer le badge « ${b.nom} » ?`)) {
    agir(() => del(`/api/badges/${b.id}`), 'Badge supprimé.')
  }
}
</script>

<template>
  <section class="carte">
    <h2>Gestion des badges</h2>

    <div class="formulaire">
      <input v-model="uid" placeholder="UID (ex : 35 4F 35 03)" />
      <input v-model="nom" placeholder="Nom de la personne" @keyup.enter="ajouter" />
      <button @click="ajouter">Ajouter</button>
    </div>

    <button class="secondaire" :disabled="!dernierRefuse" @click="reprendreRefuse">
      {{ dernierRefuse
        ? `Dernier badge refusé (${dernierRefuse.uid}) → l'ajouter`
        : 'Aucun badge refusé à ajouter' }}
    </button>

    <p v-if="message" class="succes">{{ message }}</p>
    <p v-if="erreur" class="erreur">{{ erreur }}</p>

    <div class="defile">
      <table>
        <thead>
          <tr><th>Nom</th><th>UID</th><th>Actif</th><th></th></tr>
        </thead>
        <tbody>
          <tr v-for="b in badges" :key="b.id" :class="{ inactif: !b.actif }">
            <td>{{ b.nom }}</td>
            <td><code>{{ b.uid }}</code></td>
            <td><input type="checkbox" :checked="b.actif" @change="basculer(b)" /></td>
            <td class="actions">
              <button class="secondaire" @click="renommer(b)">Renommer</button>
              <button class="danger" @click="supprimer(b)">Supprimer</button>
            </td>
          </tr>
          <tr v-if="!badges.length"><td colspan="4" class="info">Aucun badge.</td></tr>
        </tbody>
      </table>
    </div>
  </section>
</template>