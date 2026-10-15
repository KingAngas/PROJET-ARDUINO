<script setup>
import { computed } from 'vue'
import { etat } from '../store'

const SEUIL_NUIT = 800
const m = computed(() => etat.value?.meteo)
const nuit = computed(() => m.value && m.value.luminosite < SEUIL_NUIT)
</script>

<template>
  <section v-if="m" class="grille-3">
    <div class="carte">
      <h2>Température</h2>
      <p class="valeur">{{ m.temperature }} °C</p>
    </div>
    <div class="carte">
      <h2>Humidité</h2>
      <p class="valeur">{{ m.humidite }} %</p>
    </div>
    <div class="carte">
      <h2>Luminosité</h2>
      <p class="valeur">{{ m.luminosite }}</p>
      <p class="sous">{{ nuit ? '🌙 Nuit' : '☀️ Jour' }}</p>
    </div>
    <p class="info pleine-largeur">Mesure de {{ m.date.slice(11, 19) }}</p>
  </section>
  <section v-else class="carte"><p class="info">Aucune mesure reçue pour l'instant.</p></section>
</template>