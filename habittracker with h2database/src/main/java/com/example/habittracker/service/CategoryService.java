package com.example.habittracker.service;

import com.example.habittracker.model.Category;
import com.example.habittracker.repository.CategoryRepository;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.Optional;

@Service
public class CategoryService {

    private final CategoryRepository categoryRepository;

    public CategoryService(CategoryRepository categoryRepository) {
        this.categoryRepository = categoryRepository;
    }

    // Get all categories
    public List<Category> getAllCategories() {
        return categoryRepository.findAll();
    }

    // Get one category by ID
    public Optional<Category> getCategoryById(Long id) {
        return categoryRepository.findById(id);
    }

    // Save category
    public Category saveCategory(Category category) {
        return categoryRepository.save(category);
    }

    // Delete category
    public void deleteCategory(Long id) {

        Optional<Category> optionalCategory =
                categoryRepository.findById(id);

        if (optionalCategory.isPresent()) {

            Category category = optionalCategory.get();

            // Only delete if no habits belong to this category
            if (category.getHabits().isEmpty()) {

                categoryRepository.deleteById(id);
            }
        }
    }

    // Count total categories
    public long getTotalCategories() {
        return categoryRepository.count();
    }
}