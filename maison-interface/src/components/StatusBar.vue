<script setup>
import { computed } from 'vue'
import { etat, connecte } from '../store'

const enLigne = computed(() => etat.value?.esp32.enLigne)
const dernier = computed(() => etat.value?.esp32.derniereConnexion?.slice(11, 19))
</script>

<template>
  <header class="barre">
    <h1>Maison connectée</h1>
    <div class="pastilles">
      <span class="pastille" :class="enLigne ? 'ok' : 'ko'">
        ESP32 {{ enLigne ? 'en ligne' : 'hors ligne' }}
      </span>
      <span v-if="dernier" class="info">Dernier contact : {{ dernier }}</span>
      <span class="pastille" :class="connecte ? 'ok' : 'ko'">
        {{ connecte ? 'Temps réel actif' : 'Temps réel coupé' }}
      </span>
    </div>
  </header>
</template>