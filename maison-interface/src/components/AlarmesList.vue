<script setup>
import { computed } from 'vue'
import { etat } from '../store'

const alarmes = computed(() => etat.value.alarmes)
</script>

<template>
  <section class="carte">
    <h2>Alarmes</h2>
    <p v-if="!alarmes.length" class="info">Aucune alarme.</p>
    <ul class="liste">
      <li v-for="a in alarmes" :key="a.id">
        <span class="pastille ko">⚠ 3 refus d'affilée</span>
        <span class="info">{{ a.date.slice(5, 16) }}</span>
      </li>
    </ul>
  </section>
</template>