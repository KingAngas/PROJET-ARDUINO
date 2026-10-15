<script setup>
import { computed, ref } from 'vue'
import { etat } from '../store'
import { post } from '../api'

const porte = computed(() => etat.value.porte)
const envoye = ref(false)
const erreur = ref('')

async function ouvrir() {
  erreur.value = ''
  try {
    await post('/api/porte')
    envoye.value = true
    setTimeout(() => { envoye.value = false }, 4000)
  } catch (e) {
    erreur.value = e.message
  }
}
</script>

<template>
  <section class="carte">
    <h2>Porte</h2>
    <p class="valeur">{{ porte.ouverte ? '🔓 Ouverte' : '🔒 Fermée' }}</p>
    <div class="boutons">
      <button :disabled="envoye" @click="ouvrir">
        {{ envoye ? 'Ordre envoyé…' : 'Ouvrir 5 s' }}
      </button>
    </div>
    <p v-if="erreur" class="erreur">{{ erreur }}</p>
  </section>
</template>