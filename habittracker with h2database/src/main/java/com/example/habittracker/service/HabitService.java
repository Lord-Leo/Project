package com.example.habittracker.service;

import com.example.habittracker.model.Habit;
import com.example.habittracker.repository.HabitRepository;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.Optional;

@Service
public class HabitService {

    private final HabitRepository habitRepository;

    public HabitService(HabitRepository habitRepository) {
        this.habitRepository = habitRepository;
    }

    // Get all habits
    public List<Habit> getAllHabits() {
        return habitRepository.findAll();
    }

    // Get one habit by ID
    public Optional<Habit> getHabitById(Long id) {
        return habitRepository.findById(id);
    }

    // Save habit
    public Habit saveHabit(Habit habit) {
        return habitRepository.save(habit);
    }

    // Delete habit
    public void deleteHabit(Long id) {
        habitRepository.deleteById(id);
    }

    // Mark habit as completed
    public void markAsCompleted(Long id) {

        Optional<Habit> optionalHabit =
                habitRepository.findById(id);

        if (optionalHabit.isPresent()) {

            Habit habit = optionalHabit.get();

            habit.setCompleted(true);

            habitRepository.save(habit);
        }
    }

    // Count total habits
    public long getTotalHabits() {
        return habitRepository.count();
    }

    // Count completed habits
    public long getCompletedHabits() {
        return habitRepository.countByCompletedTrue();
    }

    // Count pending habits
    public long getPendingHabits() {
        return habitRepository.countByCompletedFalse();
    }
}