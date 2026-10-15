<script setup>
import { computed } from 'vue'
import { etat } from '../store'

const acces = computed(() => etat.value.acces)

function libelle(a) {
  if (a.cause === 'site' && !a.uid) return 'Ouverture depuis le site'
  return a.nom || `Badge inconnu (${a.uid})`
}
</script>

<template>
  <section class="carte">
    <h2>Derniers accès</h2>
    <p v-if="!acces.length" class="info">Aucun passage enregistré.</p>
    <ul class="liste">
      <li v-for="a in acces" :key="a.id">
        <span class="pastille" :class="a.autorise ? 'ok' : 'ko'">
          {{ a.autorise ? 'Autorisé' : 'Refusé' }}
        </span>
        <span class="texte">{{ libelle(a) }}</span>
        <span class="info">{{ a.cause === 'site' ? 'site' : 'badge' }} · {{ a.date.slice(5, 16) }}</span>
      </li>
    </ul>
  </section>
</template>