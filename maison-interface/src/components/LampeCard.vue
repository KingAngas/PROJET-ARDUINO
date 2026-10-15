<script setup>
import { computed, ref } from 'vue'
import { etat } from '../store'
import { post } from '../api'

const lampe = computed(() => etat.value.lampe)
const erreur = ref('')

const NOMS = { auto: 'Automatique', on: 'Allumée (forcée)', off: 'Éteinte (forcée)' }

async function changer(mode) {
  erreur.value = ''
  try {
    await post('/api/lampe', { mode })
  } catch (e) {
    erreur.value = e.message
  }
}
</script>

<template>
  <section class="carte">
    <h2>Lampe</h2>
    <p class="valeur">{{ lampe.allumee ? '💡 Allumée' : 'Éteinte' }}</p>
    <p class="sous">Mode : {{ NOMS[lampe.modeActuel] }}</p>
    <p v-if="lampe.modeActuel !== lampe.modeDemande" class="info">
      Ordre envoyé, en attente de l'ESP32…
    </p>
    <div class="boutons">
      <button :class="{ actif: lampe.modeDemande === 'on' }" @click="changer('on')">Allumer</button>
      <button :class="{ actif: lampe.modeDemande === 'off' }" @click="changer('off')">Éteindre</button>
      <button :class="{ actif: lampe.modeDemande === 'auto' }" @click="changer('auto')">Automatique</button>
    </div>
    <p v-if="erreur" class="erreur">{{ erreur }}</p>
  </section>
</template>