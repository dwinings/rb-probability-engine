require_relative '../lib/probability-engine/native'
require 'rspec'

TERMINATION_PROB = 0.9
TOLERANCE = 0.0001 # +/- 0.1%

describe ProbTree do
  context "sparse prob dists" do
    before(:each) do
      @prob_dists = [
      {
        a: {reward: 1, prob: 0.5},
        c: {reward: 1, prob: 0.01},
        failure: {prob: 0.49}
      },
      {
        a: {reward: 1, prob: 0.3},
        b: {reward: 1, prob: 0.4},
        failure: {prob: 0.3}
      }
    ]
    end
    it 'should compute missing goal keys in distributions' do
      p = ProbTree.new(@prob_dists, {b: 1, c: 1})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.9008951844811242)
      expect(iterations).to be(230)
    end

    it 'should handle prob_dists with no goal elements' do
      p = ProbTree.new(@prob_dists, {c: 1})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.900895184481123)
      expect(iterations).to be(230)
    end

    it 'should handle goals containing all elements' do
      p = ProbTree.new(@prob_dists, {a: 1, b: 1, c: 1})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.900895184481123)
      expect(iterations).to be(230)
    end

    it 'should handle goals containing multiples of elements' do
      p = ProbTree.new(@prob_dists, {a: 10, b: 10, c: 3})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end

      expect(p.success_prob).to be_within(TOLERANCE).of(0.9002998551225079)
      expect(iterations).to be(531)
    end

    it 'should handle goals with missing elements and multiples' do
      p = ProbTree.new(@prob_dists, {a: 10, b: 10})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.9076745582896447)
      expect(iterations).to be(33)
    end

    it 'should handle goals with useless prob_dists and multiples' do
      p = ProbTree.new(@prob_dists, {b: 30, c: 2})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.9003793730767657)
      expect(iterations).to be(388)
    end
  end

  context 'single prob dist' do
    before(:each) do
      @prob_dists = [
      {
        a: {reward: 1, prob: 0.5},
        b: {reward: 2, prob: 0.3},
        c: {reward: 3, prob: 0.1},
        failure: {prob: 0.1}
      }
    ]
    end
    it 'should handle a single prob dist with a full goal' do
      p = ProbTree.new(@prob_dists, {a: 5, b: 4, c: 5})
      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.904704869924908)
      expect(iterations).to be(38)
    end

    it 'should handle useless elements.' do
      p = ProbTree.new(@prob_dists, {a: 5, b: 4})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.905533251946875)
      expect(iterations).to be(15)
    end
  end

  context 'sparse prob dists with multiples' do
    before(:each) do
      @prob_dists = [
      {
        a: {reward: 3, prob: 0.5},
        c: {reward: 5, prob: 0.3},
        failure: {prob: 0.2}
      },
      {
        a: {reward: 2, prob: 0.3},
        b: {reward: 4, prob: 0.4},
        failure: {prob: 0.3}
      }
    ]
    end

    it 'should handle a fully specified goal with no multiples' do
      p = ProbTree.new(@prob_dists, {a: 1, b: 1, c: 1})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(iterations).to eq(8)
      expect(p.success_prob).to be_within(TOLERANCE).of(0.9262993113605856)
    end

    it 'should handle a fully specified goal with multiples' do
      p = ProbTree.new(@prob_dists, {a: 5, b: 4, c: 2})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.9225831159948961)
      expect(iterations).to be(8)
    end

    it 'should handle a sparse goal with no multiples' do
      p = ProbTree.new(@prob_dists, {a: 1, b: 1})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.9170637500000001)
      expect(iterations).to be(5)

    end

    it 'should handle a sparse goal with multiples' do
      p = ProbTree.new(@prob_dists, {a: 5, b: 4})

      iterations = 0
      while (p.success_prob < TERMINATION_PROB) do
        p.run_once
        iterations += 1
      end
      expect(p.success_prob).to be_within(TOLERANCE).of(0.9310035000000003)
      expect(iterations).to be(6)
    end
  end
end
